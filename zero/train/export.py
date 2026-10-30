"""Exports a BlokusZero network to ONNX.

Without --checkpoint, the network has random weights, for testing and
benchmarking inference.

  uv run python export.py --out ../data/models/random_6x32.onnx
"""

import argparse

import torch

import model


def export(net: model.Net, path: str) -> None:
    """Writes `net` to `path` as ONNX, with a dynamic batch size."""
    net.eval()
    planes = torch.zeros(
        2, model.NUM_PLANES, model.BOARD_SIZE, model.BOARD_SIZE)
    globals_ = torch.zeros(2, model.NUM_GLOBALS)
    batch = torch.export.Dim("batch", min=1)
    torch.onnx.export(
        net,
        (planes, globals_),
        path,
        input_names=["planes", "globals"],
        output_names=["policy", "outcome", "margin"],
        dynamic_shapes=({0: batch}, {0: batch}),
        dynamo=True,
        external_data=False,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, help="ONNX file to write.")
    parser.add_argument("--checkpoint", help="Training checkpoint to load.")
    parser.add_argument(
        "--blocks", type=int, default=model.NetConfig.num_blocks)
    parser.add_argument(
        "--channels", type=int, default=model.NetConfig.channels)
    parser.add_argument("--seed", type=int, default=0)
    args = parser.parse_args()

    torch.manual_seed(args.seed)
    config = model.NetConfig(num_blocks=args.blocks, channels=args.channels)
    net = model.Net(config)
    if args.checkpoint:
        net.load_state_dict(torch.load(args.checkpoint)["model"])
    export(net, args.out)
    macs = model.count_macs(config) / 1e6
    print(f"Wrote {args.out}: {config}, {macs:.0f}M MACs")


if __name__ == "__main__":
    main()
