"""Tests reading shards, on one written by the C++ zero:generate.

zero/testdata/selfplay.bagz is one 2-player game at 50 MCTS iterations:

  bazel run -c opt //zero:generate -- --num_games=1 --iterations=50 \
    --threads=1 --seed=0 --output=$PWD/zero/testdata/selfplay
  mv zero/testdata/selfplay-00000.bagz zero/testdata/selfplay.bagz
"""

import pathlib

import numpy as np
import pytest
import torch

import data
import model

SHARD = str(pathlib.Path(__file__).parent.parent / 'testdata' /
            'selfplay.bagz')
# The number of cells in each tile, in kTiles order.
TILE_SIZES = np.array([1, 2, 3, 3] + [4] * 5 + [5] * 12)
NUM_TILES = len(TILE_SIZES)


@pytest.fixture(scope='module')
def examples() -> dict[str, np.ndarray]:
    return data.read_examples([SHARD])


def test_one_game(examples):
    n = len(examples['color'])
    assert n > 40
    assert np.all(examples['num_players'] == 2)
    assert np.all(np.diff(examples['move_number']) > 0)
    # Colors are 1 to 4, BLUE first.
    assert examples['color'][0] == 1
    assert set(examples['color']) == {1, 2, 3, 4}


def test_shapes(examples):
    n = len(examples['color'])
    assert examples['planes'].shape == (n, model.NUM_PLANES, model.BOARD_SIZE,
                                        model.BOARD_SIZE)
    assert examples['globals'].shape == (n, model.NUM_GLOBALS)
    assert examples['target_index'].shape == (n, data.MAX_TARGETS)
    assert examples['target_prob'].shape == (n, data.MAX_TARGETS)
    assert examples['outcome'].shape == (n,)
    assert examples['margin'].shape == (n, 1)


def test_policy_targets(examples):
    for index, prob in zip(examples['target_index'], examples['target_prob']):
        k = np.count_nonzero(prob)
        assert 1 <= k <= data.MAX_TARGETS
        assert prob.sum() == pytest.approx(1, abs=1e-5)
        # Largest first, then padding.
        assert np.all(np.diff(prob[:k]) <= 0)
        assert np.all(prob[k:] == 0)
        assert np.all(index[:k] < model.POLICY_SIZE)
        assert len(set(index[:k])) == k


def test_results(examples):
    margin = np.round(examples['margin'][:, 0] * data.MARGIN_SCALE)
    outcome = examples['outcome']
    expected = np.where(margin > 0, 0, np.where(margin == 0, 1, 2))
    assert np.all(outcome == expected)
    # BLUE and RED are one player, YELLOW and GREEN the other.
    player = (examples['color'] - 1) % 2
    assert len(set(margin[player == 0])) == 1
    assert np.all(margin[player == 1] == -margin[player == 0][0])


def test_planes_match_globals(examples):
    planes = examples['planes']
    globals_ = examples['globals']
    for i in range(4):
        # Each frame color occupies the cells of the tiles it has played.
        unplayed = globals_[:, NUM_TILES * i:NUM_TILES * (i + 1)]
        played_cells = ((1 - unplayed) * TILE_SIZES).sum(axis=1)
        occupied = planes[:, i].sum(axis=(1, 2))
        np.testing.assert_array_equal(occupied, played_cells)
    # The mover has corners to play on, at the top left in its first move.
    assert np.all(planes[:, 4].sum(axis=(1, 2)) > 0)
    assert planes[0, 4, 0, 0] == 1
    # Nobody has passed at the start.
    np.testing.assert_array_equal(globals_[0, 84:], [1, 1, 1, 1])


def test_concatenates_shards():
    assert len(data.dataset([SHARD, SHARD])) == 2 * len(data.dataset([SHARD]))


def test_shuffled_batches():
    ds = data.dataset([SHARD])
    n = len(ds)
    batches = list(ds.shuffle(seed=1).batch(16, drop_remainder=True))
    assert len(batches) == n // 16
    move_numbers = np.concatenate([b['move_number'] for b in batches])
    assert len(set(move_numbers)) == len(move_numbers)
    assert not np.all(np.diff(move_numbers) > 0)
    tensors = data.to_tensors(batches[0])
    assert tensors['planes'].shape == (16, model.NUM_PLANES, model.BOARD_SIZE,
                                       model.BOARD_SIZE)
    assert tensors['planes'].dtype == torch.float32
    assert tensors['target_index'].dtype == torch.int64
    assert tensors['outcome'].dtype == torch.int64
