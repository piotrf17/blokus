"""Trains a BlokusZero network on self-play shards.

Shards are sorted by name, and the last --val_shards of them are held out
for validation (whole shards, so positions from one game don't end up on
both sides). Training shuffles the examples of all the other shards
together each epoch.

  uv run python train.py --data='../data/bootstrap/*.bagz' --out=../data/runs/NAME

--data takes a comma-separated list of shard paths or glob patterns.

Writes, in --out:
  metrics.jsonl: a line of training metrics every --log_every steps, and of
    validation metrics after each epoch.
  checkpoint.pt: the model, its config and the optimizer, after each epoch.
  model.onnx: the final model, for the C++ Evaluator.
"""

import dataclasses
import glob
import json
import math
import os
import time

from absl import app
from absl import flags
import grain
import torch
import torch.nn.functional as F

import data
import model

_DATA = flags.DEFINE_list(
    "data", None, "Shard paths or glob patterns.", required=True)
_OUT = flags.DEFINE_string("out", None, "Output directory.", required=True)
_VAL_SHARDS = flags.DEFINE_integer(
    "val_shards", 1, "Hold out this many of the last shards for validation.")
_VAL_EXAMPLES = flags.DEFINE_integer(
    "val_examples", 20000, "Validate on at most this many examples.")
_INIT = flags.DEFINE_string("init", None, "Checkpoint to start from.")
_BLOCKS = flags.DEFINE_integer(
    "blocks", model.NetConfig.num_blocks,
    "Residual blocks, without --init.")
_CHANNELS = flags.DEFINE_integer(
    "channels", model.NetConfig.channels,
    "Channels per convolution, without --init.")
_EPOCHS = flags.DEFINE_integer("epochs", 4, "Passes over the training data.")
_BATCH_SIZE = flags.DEFINE_integer("batch_size", 256, "Examples per step.")
_LR = flags.DEFINE_float("lr", 1e-3, "Peak learning rate.")
_WARMUP_STEPS = flags.DEFINE_integer(
    "warmup_steps", 200, "Steps of linear learning rate warmup.")
_WEIGHT_DECAY = flags.DEFINE_float("weight_decay", 1e-4, "AdamW weight decay.")
_MARGIN_WEIGHT = flags.DEFINE_float(
    "margin_weight", 0.1, "Weight of the margin loss.")
_THREADS = flags.DEFINE_integer("threads", 4, "PyTorch threads.")
_DATA_THREADS = flags.DEFINE_integer(
    "data_threads", 2, "Threads reading and parsing examples.")
_LOG_EVERY = flags.DEFINE_integer(
    "log_every", 100, "Log training metrics every this many steps.")
_SEED = flags.DEFINE_integer("seed", 0, "Seed for weights and shuffling.")


@dataclasses.dataclass(frozen=True)
class LossConfig:
    # The weight of the margin's Huber loss, relative to the policy and
    # outcome cross-entropies.
    margin_weight: float = 0.1


def losses(net: model.Net, batch: dict[str, torch.Tensor],
           config: LossConfig = LossConfig()) -> dict[str, torch.Tensor]:
    """Returns the losses and accuracies of `net` on a decoded batch.

    "loss" is the total to minimize. The policy loss is the cross-entropy
    between the target distribution and the softmax over all policy logits
    (illegal moves aren't masked). policy_acc is how often the largest logit
    is the target's most likely move, and outcome_acc how often the most
    likely outcome is the real one. margin_error is the mean absolute margin
    error in points.
    """
    policy, outcome, margin = net(batch["planes"], batch["globals"])
    log_probs = F.log_softmax(policy, dim=1)
    target_log_probs = log_probs.gather(1, batch["target_index"])
    policy_loss = -(batch["target_prob"] * target_log_probs).sum(dim=1).mean()
    outcome_loss = F.cross_entropy(outcome, batch["outcome"])
    margin_loss = F.huber_loss(margin, batch["margin"])
    with torch.no_grad():
        policy_acc = (policy.argmax(dim=1) ==
                      batch["target_index"][:, 0]).float().mean()
        outcome_acc = (outcome.argmax(dim=1) ==
                       batch["outcome"]).float().mean()
        margin_error = ((margin - batch["margin"]).abs().mean() *
                        data.MARGIN_SCALE)
    return {
        "loss": (policy_loss + outcome_loss +
                 config.margin_weight * margin_loss),
        "policy_loss": policy_loss,
        "outcome_loss": outcome_loss,
        "margin_loss": margin_loss,
        "policy_acc": policy_acc,
        "outcome_acc": outcome_acc,
        "margin_error": margin_error,
    }


def learning_rate(step: int, total_steps: int, peak: float,
                  warmup_steps: int) -> float:
    """Linear warmup to `peak`, then cosine decay to 0 at `total_steps`."""
    if step < warmup_steps:
        return peak * (step + 1) / warmup_steps
    progress = (step - warmup_steps) / max(1, total_steps - warmup_steps)
    return peak * 0.5 * (1 + math.cos(math.pi * min(1.0, progress)))


def batches(ds: grain.MapDataset, batch_size: int, seed: int | None,
            num_threads: int):
    """Yields decoded tensor batches of `ds`, shuffled if `seed` is set.

    Drops the last partial batch when shuffling (training), and keeps it
    otherwise (validation).
    """
    if seed is not None:
        ds = ds.shuffle(seed=seed)
    ds = ds.batch(batch_size, drop_remainder=seed is not None)
    read_options = grain.ReadOptions(
        num_threads=num_threads, prefetch_buffer_size=4 * batch_size)
    for batch in ds.to_iter_dataset(read_options):
        yield data.to_tensors(batch)


def evaluate(net: model.Net, ds: grain.MapDataset, batch_size: int,
             num_threads: int, config: LossConfig) -> dict[str, float]:
    """Returns the mean of each of `losses` over `ds`."""
    net.eval()
    totals = {}
    count = 0
    with torch.no_grad():
        for batch in batches(ds, batch_size, None, num_threads):
            n = len(batch["outcome"])
            for key, value in losses(net, batch, config).items():
                totals[key] = totals.get(key, 0.0) + value.item() * n
            count += n
    net.train()
    return {key: value / count for key, value in totals.items()}


def split_shards(patterns: list[str], val_shards: int):
    """Returns the training and validation shard paths."""
    paths = sorted({p for pattern in patterns for p in glob.glob(pattern)})
    if len(paths) <= val_shards:
        raise ValueError(f"Need more than {val_shards} shards, found "
                         f"{len(paths)}")
    if val_shards == 0:
        return paths, []
    return paths[:-val_shards], paths[-val_shards:]


def main(argv: list[str]) -> None:
    del argv  # Unused.
    torch.manual_seed(_SEED.value)
    torch.set_num_threads(_THREADS.value)
    out = _OUT.value
    os.makedirs(out, exist_ok=True)
    loss_config = LossConfig(margin_weight=_MARGIN_WEIGHT.value)
    batch_size = _BATCH_SIZE.value

    train_paths, val_paths = split_shards(_DATA.value, _VAL_SHARDS.value)
    train_ds = data.dataset(train_paths)
    val_ds = data.dataset(val_paths) if val_paths else None
    if val_ds is not None and len(val_ds) > _VAL_EXAMPLES.value:
        val_ds = val_ds[:_VAL_EXAMPLES.value]
    steps_per_epoch = len(train_ds) // batch_size
    total_steps = steps_per_epoch * _EPOCHS.value
    print(f"{len(train_ds)} training examples in {len(train_paths)} shards, "
          f"{len(val_ds) if val_ds else 0} validation examples; "
          f"{total_steps} steps")
    if steps_per_epoch == 0:
        raise ValueError("Fewer examples than one batch")

    config = model.NetConfig(num_blocks=_BLOCKS.value,
                             channels=_CHANNELS.value)
    if _INIT.value:
        checkpoint = torch.load(_INIT.value)
        config = model.NetConfig(**checkpoint["config"])
    net = model.Net(config)
    if _INIT.value:
        net.load_state_dict(checkpoint["model"])
    net.train()
    optimizer = torch.optim.AdamW(
        net.parameters(), lr=_LR.value, weight_decay=_WEIGHT_DECAY.value)

    metrics_file = open(os.path.join(out, "metrics.jsonl"), "a")

    def log(record: dict) -> None:
        metrics_file.write(json.dumps(record) + "\n")
        metrics_file.flush()
        print(" ".join(f"{k}={v:.4g}" if isinstance(v, float) else f"{k}={v}"
                       for k, v in record.items()))

    log_every = _LOG_EVERY.value
    step = 0
    for epoch in range(_EPOCHS.value):
        # Sums of the losses since the last log, over num_steps steps. The
        # first log of an epoch can cover fewer than log_every steps.
        sums = {}
        num_steps = 0
        start = time.time()
        for batch in batches(train_ds, batch_size, _SEED.value + epoch,
                             _DATA_THREADS.value):
            lr = learning_rate(step, total_steps, _LR.value,
                               _WARMUP_STEPS.value)
            for group in optimizer.param_groups:
                group["lr"] = lr
            result = losses(net, batch, loss_config)
            optimizer.zero_grad()
            result["loss"].backward()
            optimizer.step()
            step += 1
            num_steps += 1
            for key, value in result.items():
                sums[key] = sums.get(key, 0.0) + value.item()
            if step % log_every == 0:
                elapsed = time.time() - start
                log({"split": "train", "epoch": epoch, "step": step,
                     "lr": lr,
                     "examples_per_s": num_steps * batch_size / elapsed,
                     **{k: v / num_steps for k, v in sums.items()}})
                sums = {}
                num_steps = 0
                start = time.time()

        torch.save({"model": net.state_dict(),
                    "config": dataclasses.asdict(config),
                    "optimizer": optimizer.state_dict(),
                    "step": step,
                    "epoch": epoch},
                   os.path.join(out, "checkpoint.pt"))
        if val_ds is not None:
            log({"split": "val", "epoch": epoch, "step": step,
                 **evaluate(net, val_ds, batch_size, _DATA_THREADS.value,
                            loss_config)})

    model.export_onnx(net, os.path.join(out, "model.onnx"))
    metrics_file.close()
    print(f"Wrote {out}/model.onnx")


if __name__ == "__main__":
    app.run(main)
