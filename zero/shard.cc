#include "zero/shard.h"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "src/bagz_options.h"
#include "src/bagz_reader.h"
#include "src/bagz_writer.h"

#include "zero/features.h"

namespace blokus {
namespace {

// The zstd compression level, the bagz default.
constexpr int kZstdLevel = 3;

// Packs 0/1 floats into bits, in numpy.unpackbits order.
std::string PackBits(absl::Span<const float> values) {
  std::string bytes((values.size() + 7) / 8, '\0');
  for (size_t i = 0; i < values.size(); ++i) {
    if (values[i] != 0) bytes[i / 8] |= 0x80 >> (i % 8);
  }
  return bytes;
}

}  // namespace

TrainingExample MakeExample(const Game& game, absl::Span<const Move> moves,
                            absl::Span<const double> weights) {
  CHECK(!moves.empty());
  CHECK_EQ(moves.size(), weights.size());
  TrainingExample example;
  example.set_color(game.current_color());
  example.set_move_number(game.moves().size());
  example.set_num_players(game.num_players());

  std::vector<float> planes(kPlanesSize);
  std::vector<float> globals(kNumGlobals);
  EncodeInputs(game, absl::MakeSpan(planes), absl::MakeSpan(globals));
  example.set_planes(PackBits(planes));
  example.set_globals(PackBits(globals));

  // The moves by decreasing weight, keeping the first kMaxTargets.
  std::vector<int> order(moves.size());
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(),
                   [&](int a, int b) { return weights[a] > weights[b]; });
  order.resize(std::min<int>(order.size(), kMaxTargets));
  double total = 0;
  for (int i : order) total += weights[i];
  for (int i : order) {
    if (total > 0 && weights[i] == 0) break;
    CHECK_GE(weights[i], 0);
    CHECK_EQ(moves[i].color, game.current_color());
    example.add_target_index(PolicyIndex(moves[i]));
    example.add_target_prob(total > 0 ? weights[i] / total
                                      : 1.0 / order.size());
  }
  return example;
}

void SetResult(const GameResult& result, TrainingExample& example) {
  const int num_players = example.num_players();
  CHECK_EQ(result.scores.size(), num_players);
  const int player = (example.color() - BLUE) % num_players;
  int best_other = std::numeric_limits<int>::min();
  for (int i = 0; i < num_players; ++i) {
    if (i != player) best_other = std::max(best_other, result.scores[i]);
  }
  const int margin = result.scores[player] - best_other;
  example.set_margin(margin);
  example.set_outcome(margin > 0 ? WIN : margin == 0 ? DRAW : LOSS);
}

absl::Status WriteShard(const std::string& path,
                        absl::Span<const TrainingExample> examples) {
  // Bagz picks compression from the extension, and the temporary file
  // doesn't end in .bagz.
  const std::string tmp_path = path + ".tmp";
  absl::StatusOr<bagz::BagzWriter> writer = bagz::BagzWriter::OpenFile(
      tmp_path, {.compression = bagz::CompressionZstd{.level = kZstdLevel}});
  if (!writer.ok()) return writer.status();
  for (const TrainingExample& example : examples) {
    if (absl::Status status = writer->Write(example.SerializeAsString());
        !status.ok()) {
      return status;
    }
  }
  if (absl::Status status = writer->Close(); !status.ok()) return status;
  if (std::rename(tmp_path.c_str(), path.c_str()) != 0) {
    return absl::InternalError(absl::StrCat("Can't rename ", tmp_path));
  }
  return absl::OkStatus();
}

absl::StatusOr<std::vector<TrainingExample>> ReadShard(
    const std::string& path) {
  absl::StatusOr<bagz::BagzReader> reader = bagz::BagzReader::Open(
      path, {.compression = bagz::CompressionZstd{}});
  if (!reader.ok()) return reader.status();
  absl::StatusOr<std::vector<std::string>> records = reader->Read();
  if (!records.ok()) return records.status();
  std::vector<TrainingExample> examples(records->size());
  for (size_t i = 0; i < records->size(); ++i) {
    if (!examples[i].ParseFromString((*records)[i])) {
      return absl::DataLossError(absl::StrCat("Bad example in ", path));
    }
  }
  return examples;
}

}  // namespace blokus
