"""Reads the training data shards that zero:generate writes.

A shard is a Bagz file (https://github.com/google-deepmind/bagz) of
serialized TrainingExamples (zero/training.proto). training_pb2.py is
generated from that file, with the command in it. Bagz reads any example
cheaply, so Grain can shuffle examples across all the shards.
"""

import bagz
import grain
import numpy as np
import torch

import model
import training_pb2

# The most moves an example's policy target keeps (kMaxTargets in
# zero/shard.h).
MAX_TARGETS = 32
# The network predicts margins in units of this many points.
MARGIN_SCALE = 20


def parse_example(serialized: bytes) -> dict[str, np.ndarray]:
    """Decodes a serialized TrainingExample into the network's inputs and
    training targets.

    Returns planes [NUM_PLANES, BOARD_SIZE, BOARD_SIZE] and globals
    [NUM_GLOBALS] as float32; the policy target as target_index (int64) and
    target_prob [MAX_TARGETS], padded with probability 0; the outcome (int64,
    0 win, 1 draw, 2 loss); and the margin over MARGIN_SCALE [1]. Also the
    color, move_number and num_players, for analysis.
    """
    example = training_pb2.TrainingExample.FromString(serialized)
    planes = np.unpackbits(np.frombuffer(example.planes, dtype=np.uint8))
    globals_ = np.unpackbits(np.frombuffer(example.globals, dtype=np.uint8))
    k = len(example.target_index)
    target_index = np.zeros(MAX_TARGETS, dtype=np.int64)
    target_prob = np.zeros(MAX_TARGETS, dtype=np.float32)
    target_index[:k] = example.target_index
    target_prob[:k] = example.target_prob
    return {
        'planes': planes.reshape(
            model.NUM_PLANES, model.BOARD_SIZE, model.BOARD_SIZE).astype(
                np.float32),
        'globals': globals_.astype(np.float32),
        'target_index': target_index,
        'target_prob': target_prob,
        'outcome': np.int64(example.outcome),
        'margin': np.array([example.margin / MARGIN_SCALE], dtype=np.float32),
        'color': np.int64(example.color),
        'move_number': np.int64(example.move_number),
        'num_players': np.int64(example.num_players),
    }


def dataset(paths: list[str]) -> grain.MapDataset:
    """Returns the examples of all the shards, parsed, in file order."""
    shards = [grain.MapDataset.source(bagz.Reader(path)) for path in paths]
    return grain.MapDataset.concatenate(shards).map(parse_example)


def read_examples(paths: list[str]) -> dict[str, np.ndarray]:
    """Returns all the examples of the shards, stacked."""
    examples = list(dataset(paths))
    return {key: np.stack([e[key] for e in examples]) for key in examples[0]}


def to_tensors(batch: dict[str, np.ndarray]) -> dict[str, torch.Tensor]:
    """Converts a batch of stacked examples to tensors."""
    return {key: torch.from_numpy(np.asarray(value))
            for key, value in batch.items()}
