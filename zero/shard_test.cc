#include "zero/shard.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "game/board.h"
#include "game/game.h"
#include "zero/features.h"
#include "zero/training.pb.h"

namespace blokus {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::FloatEq;
using ::testing::FloatNear;
using ::testing::Gt;
using ::testing::SizeIs;

bool Bit(const std::string& bytes, int i) {
  return (static_cast<uint8_t>(bytes[i / 8]) >> (7 - i % 8)) & 1;
}

// A 2-player game a few moves in, with YELLOW to move.
Game TestGame() {
  Game game(2);
  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(game.MakeMove(game.PossibleMove(game.NumPossibleMoves() / 2)));
  }
  return game;
}

TEST(ShardTest, ExampleHasInputsAsBits) {
  const Game game = TestGame();
  const std::vector<Move> moves = game.PossibleMoves();
  const TrainingExample example =
      MakeExample(game, moves, std::vector<double>(moves.size(), 1));
  EXPECT_THAT(example.color(), Eq(YELLOW));
  EXPECT_THAT(example.num_players(), Eq(2));
  EXPECT_THAT(example.move_number(), Eq(5));
  ASSERT_THAT(example.planes(), SizeIs(kPlanesSize / 8));
  ASSERT_THAT(example.globals(), SizeIs(kNumGlobals / 8));

  std::vector<float> planes(kPlanesSize);
  std::vector<float> globals(kNumGlobals);
  EncodeInputs(game, absl::MakeSpan(planes), absl::MakeSpan(globals));
  for (int i = 0; i < kPlanesSize; ++i) {
    ASSERT_THAT(Bit(example.planes(), i), Eq(planes[i] == 1)) << i;
  }
  for (int i = 0; i < kNumGlobals; ++i) {
    ASSERT_THAT(Bit(example.globals(), i), Eq(globals[i] == 1)) << i;
  }
}

TEST(ShardTest, TargetKeepsLargestWeights) {
  const Game game = TestGame();
  const std::vector<Move> moves = game.PossibleMoves();
  ASSERT_THAT(moves.size(), Gt(kMaxTargets + 2));
  // Moves 0 to kMaxTargets have weights 1 to kMaxTargets + 1, and the
  // rest 0.
  std::vector<double> weights(moves.size(), 0);
  for (int i = 0; i <= kMaxTargets; ++i) weights[i] = i + 1;
  const TrainingExample example = MakeExample(game, moves, weights);
  ASSERT_THAT(example.target_index(), SizeIs(kMaxTargets));
  ASSERT_THAT(example.target_prob(), SizeIs(kMaxTargets));
  // Kept: weights 2 to kMaxTargets + 1, largest first.
  const double total = (kMaxTargets + 1) * (kMaxTargets + 2) / 2 - 1;
  float sum = 0;
  for (int i = 0; i < kMaxTargets; ++i) {
    EXPECT_THAT(example.target_index(i),
                Eq(PolicyIndex(moves[kMaxTargets - i])));
    EXPECT_THAT(example.target_prob(i),
                FloatNear((kMaxTargets + 1 - i) / total, 1e-6));
    sum += example.target_prob(i);
  }
  EXPECT_THAT(sum, FloatNear(1, 1e-5));
}

TEST(ShardTest, TargetSkipsZeroWeights) {
  const Game game = TestGame();
  const std::vector<Move> moves = game.PossibleMoves();
  std::vector<double> weights(moves.size(), 0);
  weights[3] = 30;
  weights[7] = 10;
  const TrainingExample example = MakeExample(game, moves, weights);
  ASSERT_THAT(example.target_index(), SizeIs(2));
  ASSERT_THAT(example.target_prob(), SizeIs(2));
  EXPECT_THAT(example.target_index(0), Eq(PolicyIndex(moves[3])));
  EXPECT_THAT(example.target_prob(0), FloatEq(0.75));
  EXPECT_THAT(example.target_index(1), Eq(PolicyIndex(moves[7])));
  EXPECT_THAT(example.target_prob(1), FloatEq(0.25));
}

TEST(ShardTest, TargetWithoutWeightsIsUniform) {
  const Game game = TestGame();
  const std::vector<Move> moves = {game.PossibleMove(0), game.PossibleMove(1)};
  const TrainingExample example = MakeExample(game, moves, {0, 0});
  ASSERT_THAT(example.target_prob(), SizeIs(2));
  EXPECT_THAT(example.target_prob(0), FloatEq(0.5));
  EXPECT_THAT(example.target_prob(1), FloatEq(0.5));
}

TEST(ShardTest, SetResultIsForTheMoversPlayer) {
  Game game(2);
  // BLUE and RED are player 0; YELLOW and GREEN are player 1.
  TrainingExample blue = MakeExample(game, {game.PossibleMove(0)}, {1});
  ASSERT_TRUE(game.MakeMove(game.PossibleMove(0)));
  TrainingExample yellow = MakeExample(game, {game.PossibleMove(0)}, {1});
  ASSERT_TRUE(game.MakeMove(game.PossibleMove(0)));
  TrainingExample red = MakeExample(game, {game.PossibleMove(0)}, {1});

  const GameResult result = {.winner_id = 1, .scores = {-40, -27}};
  SetResult(result, blue);
  SetResult(result, yellow);
  SetResult(result, red);
  EXPECT_THAT(blue.outcome(), Eq(LOSS));
  EXPECT_THAT(blue.margin(), Eq(-13));
  EXPECT_THAT(red.outcome(), Eq(LOSS));
  EXPECT_THAT(yellow.outcome(), Eq(WIN));
  EXPECT_THAT(yellow.margin(), Eq(13));

  SetResult({.winner_id = 0, .scores = {-5, -5}}, blue);
  EXPECT_THAT(blue.outcome(), Eq(DRAW));
  EXPECT_THAT(blue.margin(), Eq(0));
}

TEST(ShardTest, SetResultFor4Players) {
  Game game(4);
  ASSERT_TRUE(game.MakeMove(game.PossibleMove(0)));
  ASSERT_TRUE(game.MakeMove(game.PossibleMove(0)));
  TrainingExample red = MakeExample(game, {game.PossibleMove(0)}, {1});
  SetResult({.winner_id = 2, .scores = {-30, -20, -10, -50}}, red);
  EXPECT_THAT(red.outcome(), Eq(WIN));
  EXPECT_THAT(red.margin(), Eq(10));
}

TEST(ShardTest, WriteAndRead) {
  const std::string path =
      std::string(getenv("TEST_TMPDIR")) + "/shard.bagz";
  const Game game = TestGame();
  const std::vector<Move> moves = game.PossibleMoves();
  std::vector<TrainingExample> examples;
  for (int i = 0; i < 3; ++i) {
    std::vector<double> weights(moves.size(), 0);
    weights[i] = 1;
    examples.push_back(MakeExample(game, moves, weights));
  }
  ASSERT_TRUE(WriteShard(path, examples).ok());
  absl::StatusOr<std::vector<TrainingExample>> read = ReadShard(path);
  ASSERT_TRUE(read.ok()) << read.status();
  ASSERT_THAT(*read, SizeIs(3));
  for (int i = 0; i < 3; ++i) {
    EXPECT_THAT((*read)[i].target_index(), ElementsAre(PolicyIndex(moves[i])));
    EXPECT_THAT((*read)[i].planes(), Eq(examples[i].planes()));
  }
}

// Writing replaces the shard.
TEST(ShardTest, WriteReplaces) {
  const std::string path =
      std::string(getenv("TEST_TMPDIR")) + "/replaced.bagz";
  const Game game = TestGame();
  const TrainingExample example =
      MakeExample(game, {game.PossibleMove(0)}, {1});
  ASSERT_TRUE(WriteShard(path, {example, example}).ok());
  ASSERT_TRUE(WriteShard(path, {example}).ok());
  absl::StatusOr<std::vector<TrainingExample>> read = ReadShard(path);
  ASSERT_TRUE(read.ok()) << read.status();
  EXPECT_THAT(*read, SizeIs(1));
}

TEST(ShardTest, ReadsEmptyFile) {
  const std::string path = std::string(getenv("TEST_TMPDIR")) + "/empty.bagz";
  ASSERT_TRUE(WriteShard(path, {}).ok());
  absl::StatusOr<std::vector<TrainingExample>> read = ReadShard(path);
  ASSERT_TRUE(read.ok()) << read.status();
  EXPECT_THAT(*read, SizeIs(0));
}

TEST(ShardTest, ReadFailsForMissingFile) {
  EXPECT_THAT(ReadShard("/nonexistent/shard.bagz").ok(), Eq(false));
}

TEST(ShardTest, ReadFailsForGarbage) {
  const std::string path = std::string(getenv("TEST_TMPDIR")) + "/bad.bagz";
  std::ofstream(path) << "\xff\xff\xff not a proto";
  EXPECT_THAT(ReadShard(path).ok(), Eq(false));
}

}  // namespace
}  // namespace blokus
