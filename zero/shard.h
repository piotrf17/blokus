#ifndef BLOKUS_ZERO_SHARD_H_
#define BLOKUS_ZERO_SHARD_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"

#include "game/board.h"
#include "game/game.h"
#include "zero/training.pb.h"

namespace blokus {

// Training data shards for BlokusZero: Bagz files
// (https://github.com/google-deepmind/bagz) of serialized TrainingExamples
// (see zero/training.proto), each compressed with zstd. zero/train/data.py
// reads them with the bagz package and Grain. Shards use the extension
// .bagz, which tells bagz readers the records are compressed. Bagz files
// can't be appended to, so a shard is written all at once.

// The most moves an example's policy target keeps.
inline constexpr int kMaxTargets = 32;

// Returns an example of `game`'s position, with the policy target `weights`
// (e.g. visit counts) over `moves`, which must be the current color's
// possible moves (not a pass). Keeps the kMaxTargets largest weights and
// normalizes them to sum to 1. If every weight is 0, the moves share the
// target equally. The outcome and margin are left for SetResult.
TrainingExample MakeExample(const Game& game, absl::Span<const Move> moves,
                            absl::Span<const double> weights);

// Sets the outcome and margin of `example` from its game's result.
void SetResult(const GameResult& result, TrainingExample& example);

// Writes `examples` to a new shard at `path`, replacing any file there. It
// writes `path`.tmp first and renames it, so readers never see a partial
// shard.
absl::Status WriteShard(const std::string& path,
                        absl::Span<const TrainingExample> examples);

// Reads every example in the shard at `path`.
absl::StatusOr<std::vector<TrainingExample>> ReadShard(
    const std::string& path);

}  // namespace blokus

#endif
