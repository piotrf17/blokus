"""Tests for training, on the shard data_test.py uses."""

import json
import pathlib
import shutil

from absl import flags
from absl.testing import flagsaver
import numpy as np
import onnxruntime
import pytest
import torch
import torch.nn.functional as F

import data
import model
import train

SHARD = pathlib.Path(__file__).parent.parent / "testdata" / "selfplay.bagz"


def test_policy_loss_is_cross_entropy():
    net = model.Net(model.NetConfig(num_blocks=1, channels=4))
    batch = data.to_tensors(
        {k: v[:2] for k, v in _stacked(data.dataset([str(SHARD)])).items()})
    result = train.losses(net, batch)
    policy, _, _ = net(batch["planes"], batch["globals"])
    log_probs = F.log_softmax(policy, dim=1)
    expected = 0.0
    for i in range(2):
        for index, prob in zip(batch["target_index"][i],
                               batch["target_prob"][i]):
            expected -= prob * log_probs[i, index]
    assert result["policy_loss"].item() == pytest.approx(
        expected.item() / 2, rel=1e-5)
    assert result["loss"].item() == pytest.approx(
        (result["policy_loss"] + result["outcome_loss"] +
         0.1 * result["margin_loss"]).item(), rel=1e-5)


def test_learning_rate():
    assert train.learning_rate(0, 100, 1.0, 10) == pytest.approx(0.1)
    assert train.learning_rate(9, 100, 1.0, 10) == pytest.approx(1.0)
    assert train.learning_rate(55, 100, 1.0, 10) == pytest.approx(0.5)
    assert train.learning_rate(100, 100, 1.0, 10) == pytest.approx(0.0)


def test_split_shards(tmp_path):
    for name in ["b.bagz", "a.bagz", "c.bagz"]:
        (tmp_path / name).touch()
    train_paths, val_paths = train.split_shards(
        [str(tmp_path / "*.bagz")], 1)
    assert [pathlib.Path(p).name for p in train_paths] == ["a.bagz", "b.bagz"]
    assert [pathlib.Path(p).name for p in val_paths] == ["c.bagz"]
    with pytest.raises(ValueError):
        train.split_shards([str(tmp_path / "*.bagz")], 3)


def test_trains_and_exports(tmp_path):
    shards = tmp_path / "shards"
    shards.mkdir()
    for i in range(2):
        shutil.copy(SHARD, shards / f"selfplay-{i:05d}.bagz")
    out = tmp_path / "run"
    # A tiny net starts to fit the one game in the training shard.
    with flagsaver.flagsaver():
        argv = flags.FLAGS([
            "train", f"--data={shards / '*.bagz'}", f"--out={out}",
            "--blocks=1", "--channels=8", "--epochs=30", "--batch_size=16",
            "--lr=1e-2", "--warmup_steps=5", "--log_every=3", "--threads=1",
            "--data_threads=1"])
        train.main(argv)

    records = [json.loads(line)
               for line in (out / "metrics.jsonl").read_text().splitlines()]
    train_losses = [r["policy_loss"] for r in records if r["split"] == "train"]
    val = [r for r in records if r["split"] == "val"]
    assert len(val) == 30
    assert train_losses[-1] < 0.75 * train_losses[0]
    assert val[-1]["loss"] < val[0]["loss"]

    # The checkpoint loads back, and the ONNX model matches it.
    checkpoint = torch.load(out / "checkpoint.pt")
    assert checkpoint["config"]["channels"] == 8
    net = model.Net(model.NetConfig(**checkpoint["config"])).eval()
    net.load_state_dict(checkpoint["model"])
    batch = data.to_tensors(_stacked(data.dataset([str(SHARD)])))
    with torch.no_grad():
        policy, _, _ = net(batch["planes"], batch["globals"])
    session = onnxruntime.InferenceSession(str(out / "model.onnx"))
    onnx_policy, _, _ = session.run(
        None, {"planes": batch["planes"].numpy(),
               "globals": batch["globals"].numpy()})
    np.testing.assert_allclose(onnx_policy, policy.numpy(), atol=1e-4)


def _stacked(ds) -> dict[str, np.ndarray]:
    examples = list(ds)
    return {k: np.stack([e[k] for e in examples]) for k in examples[0]}
