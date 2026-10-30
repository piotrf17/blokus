"""The BlokusZero network: a small ResNet with policy and value heads.

The input encoding and the policy layout are defined by the C++ feature
encoder (zero/features.h); the constants here must match it.
"""

import dataclasses

import torch
from torch import nn

BOARD_SIZE = 20
# Occupied cells, live corners and available cells, for each of 4 colors.
NUM_PLANES = 12
# Tiles left for each of 4 colors (4 x 21), and whether each color can move.
NUM_GLOBALS = 88
# All orientations of all 21 tiles. A policy logit is an (orientation, cell)
# pair, at index orientation * BOARD_SIZE**2 + row * BOARD_SIZE + col.
NUM_ORIENTATIONS = 91
POLICY_SIZE = NUM_ORIENTATIONS * BOARD_SIZE * BOARD_SIZE
# Win, draw and loss for the player to move.
NUM_OUTCOMES = 3


@dataclasses.dataclass(frozen=True)
class NetConfig:
    num_blocks: int = 6
    channels: int = 32
    value_hidden: int = 64


class ResBlock(nn.Module):

    def __init__(self, channels: int):
        super().__init__()
        self.conv1 = nn.Conv2d(channels, channels, 3, padding=1, bias=False)
        self.bn1 = nn.BatchNorm2d(channels)
        self.conv2 = nn.Conv2d(channels, channels, 3, padding=1, bias=False)
        self.bn2 = nn.BatchNorm2d(channels)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        y = torch.relu(self.bn1(self.conv1(x)))
        y = self.bn2(self.conv2(y))
        return torch.relu(x + y)


class Net(nn.Module):
    """Maps (planes, globals) to (policy logits, outcome logits, margin).

    planes: [batch, NUM_PLANES, BOARD_SIZE, BOARD_SIZE]
    globals: [batch, NUM_GLOBALS]

    Returns policy logits [batch, POLICY_SIZE], unmasked; outcome logits
    [batch, NUM_OUTCOMES]; and the mover's predicted score margin over 20
    [batch, 1].
    """

    def __init__(self, config: NetConfig = NetConfig()):
        super().__init__()
        self.config = config
        c = config.channels
        self.stem = nn.Conv2d(NUM_PLANES, c, 3, padding=1, bias=False)
        # The global features become a per-channel bias after the stem.
        self.globals = nn.Linear(NUM_GLOBALS, c)
        self.stem_bn = nn.BatchNorm2d(c)
        self.blocks = nn.Sequential(
            *[ResBlock(c) for _ in range(config.num_blocks)])
        self.policy = nn.Conv2d(c, NUM_ORIENTATIONS, 1)
        # Mean and max pooling, then an MLP.
        self.value = nn.Sequential(
            nn.Linear(2 * c, config.value_hidden),
            nn.ReLU(),
            nn.Linear(config.value_hidden, NUM_OUTCOMES + 1),
        )

    def forward(
        self, planes: torch.Tensor, globals_: torch.Tensor
    ) -> tuple[torch.Tensor, torch.Tensor, torch.Tensor]:
        x = self.stem_bn(self.stem(planes))
        x = x + self.globals(globals_)[:, :, None, None]
        x = self.blocks(torch.relu(x))
        policy = self.policy(x).flatten(1)
        pooled = torch.cat([x.mean(dim=(2, 3)), x.amax(dim=(2, 3))], dim=1)
        value = self.value(pooled)
        return policy, value[:, :NUM_OUTCOMES], value[:, NUM_OUTCOMES:]


def count_macs(config: NetConfig) -> int:
    """Multiply-accumulates per evaluation (convolutions and linear layers)."""
    cells = BOARD_SIZE * BOARD_SIZE
    c = config.channels
    conv3 = cells * c * c * 9
    return (
        cells * NUM_PLANES * c * 9  # Stem.
        + NUM_GLOBALS * c
        + 2 * config.num_blocks * conv3
        + cells * c * NUM_ORIENTATIONS  # Policy.
        + 2 * c * config.value_hidden
        + config.value_hidden * (NUM_OUTCOMES + 1)
    )
