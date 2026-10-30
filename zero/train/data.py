"""Reads the training data shards that zero:generate writes.

A shard is a Bagz file (https://github.com/google-deepmind/bagz) of
serialized TrainingExamples (zero/training.proto). training_pb2.py is
generated from that file, with the command in it. Bagz reads any example
cheaply, so Grain can shuffle examples across all the shards.

Examples are parsed one at a time (parse_example, in a Grain map) with the
inputs still packed as bits, then batched and unpacked a batch at a time
(decode_batch), which is much faster than unpacking each example.
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
    """Parses a serialized TrainingExample.

    Returns the planes and globals as packed bits (uint8), the policy target
    as target_index (int64) and target_prob [MAX_TARGETS], padded with
    probability 0, and the outcome, margin (in points), color, move_number
    and num_players as scalars.
    """
    example = training_pb2.TrainingExample.FromString(serialized)
    k = len(example.target_index)
    target_index = np.zeros(MAX_TARGETS, dtype=np.int64)
    target_prob = np.zeros(MAX_TARGETS, dtype=np.float32)
    target_index[:k] = np.fromiter(example.target_index, np.int64, k)
    target_prob[:k] = np.fromiter(example.target_prob, np.float32, k)
    return {
        "planes": np.frombuffer(example.planes, dtype=np.uint8),
        "globals": np.frombuffer(example.globals, dtype=np.uint8),
        "target_index": target_index,
        "target_prob": target_prob,
        "outcome": np.int64(example.outcome),
        "margin": np.int64(example.margin),
        "color": np.int64(example.color),
        "move_number": np.int64(example.move_number),
        "num_players": np.int64(example.num_players),
    }


def decode_batch(batch: dict[str, np.ndarray]) -> dict[str, np.ndarray]:
    """Unpacks a batch of parsed examples into the network's inputs and
    training targets.

    Returns planes [n, NUM_PLANES, BOARD_SIZE, BOARD_SIZE] and globals
    [n, NUM_GLOBALS] as float32, and the margin over MARGIN_SCALE [n, 1].
    The other fields are as parse_example returns them, stacked.
    """
    n = len(batch["outcome"])
    decoded = dict(batch)
    decoded["planes"] = np.unpackbits(batch["planes"], axis=1).reshape(
        n, model.NUM_PLANES, model.BOARD_SIZE, model.BOARD_SIZE).astype(
            np.float32)
    decoded["globals"] = np.unpackbits(batch["globals"], axis=1).astype(
        np.float32)
    decoded["margin"] = (batch["margin"].astype(np.float32)[:, None] /
                         MARGIN_SCALE)
    return decoded


def dataset(paths: list[str]) -> grain.MapDataset:
    """Returns the parsed examples of all the shards, in file order."""
    shards = [grain.MapDataset.source(bagz.Reader(path)) for path in paths]
    return grain.MapDataset.concatenate(shards).map(parse_example)


def read_examples(paths: list[str]) -> dict[str, np.ndarray]:
    """Returns all the examples of the shards, as one decoded batch."""
    examples = list(dataset(paths))
    return decode_batch(
        {key: np.stack([e[key] for e in examples]) for key in examples[0]})


def to_tensors(batch: dict[str, np.ndarray]) -> dict[str, torch.Tensor]:
    """Decodes a batch of parsed examples into tensors."""
    return {key: torch.from_numpy(np.ascontiguousarray(value))
            for key, value in decode_batch(batch).items()}
