"""Writes the tiny model and expected outputs for zero/evaluator_test.cc.

  uv run python make_testdata.py
"""

import pathlib

import torch

import model

TESTDATA = pathlib.Path(__file__).parent.parent / "testdata"
BATCH = 3
POLICY_INDICES = [0, 777, 12345, model.POLICY_SIZE - 1]


def test_inputs() -> tuple[torch.Tensor, torch.Tensor]:
    """Fixed 0/1 inputs, also computed by TestInputs in evaluator_test.cc."""
    planes = torch.zeros(BATCH, model.NUM_PLANES * 400)
    globals_ = torch.zeros(BATCH, model.NUM_GLOBALS)
    for b in range(BATCH):
        for i in range(planes.shape[1]):
            planes[b, i] = float((b * 7 + i * 13) % 5 == 0)
        for i in range(model.NUM_GLOBALS):
            globals_[b, i] = float((b + i * 3) % 4 == 0)
    return planes.reshape(BATCH, model.NUM_PLANES, 20, 20), globals_


def main() -> None:
    torch.manual_seed(0)
    net = model.Net(model.NetConfig(num_blocks=2, channels=8))
    # Non-trivial batch norm statistics.
    net.train()
    with torch.no_grad():
        for _ in range(3):
            net(torch.rand(8, model.NUM_PLANES, 20, 20),
                torch.rand(8, model.NUM_GLOBALS))
    model.export_onnx(net, str(TESTDATA / "tiny.onnx"))

    with torch.no_grad():
        policy, outcome, margin = net(*test_inputs())
    lines = []
    for b in range(BATCH):
        lines.append(f"policy_sum {b} {policy[b].double().sum().item():.6f}")
        for i in POLICY_INDICES:
            lines.append(f"policy {b} {i} {policy[b, i].item():.6f}")
        values = " ".join(f"{v:.6f}" for v in outcome[b].tolist())
        lines.append(f"outcome {b} {values}")
        lines.append(f"margin {b} {margin[b, 0].item():.6f}")
    (TESTDATA / "tiny_expected.txt").write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
