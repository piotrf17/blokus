"""Exports a BlokusZero network to ONNX.

Without --checkpoint, the network has random weights, for testing and
benchmarking inference. With one (from train.py), the network's size comes
from the checkpoint.

  uv run python export.py --out ../data/models/random_6x32.onnx
"""

from absl import app
from absl import flags
import torch

import model

_OUT = flags.DEFINE_string("out", None, "ONNX file to write.", required=True)
_CHECKPOINT = flags.DEFINE_string(
    "checkpoint", None, "Training checkpoint to load.")
_BLOCKS = flags.DEFINE_integer(
    "blocks", model.NetConfig.num_blocks, "Residual blocks.")
_CHANNELS = flags.DEFINE_integer(
    "channels", model.NetConfig.channels, "Channels per convolution.")
_SEED = flags.DEFINE_integer("seed", 0, "Seed for the random weights.")


def main(argv: list[str]) -> None:
    del argv  # Unused.
    torch.manual_seed(_SEED.value)
    config = model.NetConfig(num_blocks=_BLOCKS.value,
                             channels=_CHANNELS.value)
    if _CHECKPOINT.value:
        checkpoint = torch.load(_CHECKPOINT.value)
        config = model.NetConfig(**checkpoint["config"])
    net = model.Net(config)
    if _CHECKPOINT.value:
        net.load_state_dict(checkpoint["model"])
    model.export_onnx(net, _OUT.value)
    macs = model.count_macs(config) / 1e6
    print(f"Wrote {_OUT.value}: {config}, {macs:.0f}M MACs")


if __name__ == "__main__":
    app.run(main)
