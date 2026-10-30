"""Tests for the network and its ONNX export.

  uv run pytest
"""

import numpy as np
import onnxruntime
import pytest
import torch

import export
import model


def random_inputs(batch: int) -> tuple[torch.Tensor, torch.Tensor]:
    generator = torch.Generator().manual_seed(1)
    planes = torch.randint(
        0, 2, (batch, model.NUM_PLANES, model.BOARD_SIZE, model.BOARD_SIZE),
        generator=generator).float()
    globals_ = torch.randint(
        0, 2, (batch, model.NUM_GLOBALS), generator=generator).float()
    return planes, globals_


def test_output_shapes():
    net = model.Net(model.NetConfig(num_blocks=2, channels=8)).eval()
    policy, outcome, margin = net(*random_inputs(3))
    assert policy.shape == (3, model.POLICY_SIZE)
    assert outcome.shape == (3, model.NUM_OUTCOMES)
    assert margin.shape == (3, 1)


def test_policy_layout():
    # Logit orientation * 400 + row * 20 + col is that orientation's policy
    # channel at (row, col).
    net = model.Net(model.NetConfig(num_blocks=1, channels=8)).eval()
    planes, globals_ = random_inputs(1)
    policy, _, _ = net(planes, globals_)
    trunk = net.blocks(torch.relu(
        net.stem_bn(net.stem(planes))
        + net.globals(globals_)[:, :, None, None]))
    channels = net.policy(trunk)
    assert torch.allclose(
        policy[0, 7 * 400 + 3 * 20 + 11], channels[0, 7, 3, 11])


def test_count_macs():
    # 6 blocks x 32 channels: 12 3x3 convolutions dominate.
    macs = model.count_macs(model.NetConfig(num_blocks=6, channels=32))
    assert macs == pytest.approx(12 * 400 * 32 * 32 * 9, rel=0.2)


@pytest.mark.parametrize("batch", [1, 5])
def test_onnx_matches_torch(tmp_path, batch):
    torch.manual_seed(0)
    net = model.Net(model.NetConfig(num_blocks=2, channels=16))
    # Non-trivial batch norm statistics, so that eval mode matters.
    net.train()
    with torch.no_grad():
        for _ in range(3):
            net(*random_inputs(8))
    path = str(tmp_path / "net.onnx")
    export.export(net, path)

    planes, globals_ = random_inputs(batch)
    with torch.no_grad():
        expected = net(planes, globals_)
    session = onnxruntime.InferenceSession(path)
    actual = session.run(
        ["policy", "outcome", "margin"],
        {"planes": planes.numpy(), "globals": globals_.numpy()})
    for e, a in zip(expected, actual):
        np.testing.assert_allclose(a, e.numpy(), rtol=1e-4, atol=1e-5)
