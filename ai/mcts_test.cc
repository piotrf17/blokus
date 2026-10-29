#include "ai/mcts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "game/game.h"
#include "game/game_runner.h"
#include "game/player.h"
#include "game/tile.h"

namespace blokus {
namespace {

using ::testing::DoubleEq;
using ::testing::DoubleNear;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::Not;
using ::testing::SizeIs;

MctsOptions SmallOptions(int num_iterations, uint64_t seed,
                         int num_threads = 1) {
  MctsOptions options;
  options.num_iterations = num_iterations;
  options.num_threads = num_threads;
  options.seed = seed;
  return options;
}

// Plays a full game between MctsAIs, and returns the moves. Player i is
// seeded with `options.seed + i`.
std::vector<Move> PlayMctsGame(int num_players, const MctsOptions& options) {
  GameRunner runner(num_players);
  for (int id = 0; id < num_players; ++id) {
    MctsOptions player_options = options;
    player_options.seed = *options.seed + id;
    runner.AddPlayer(std::make_unique<MctsAI>(id, player_options));
  }
  std::vector<Move> moves;
  runner.AddObserver(
      [&moves](const Game&, const Move& move) { moves.push_back(move); });
  runner.Play();
  return moves;
}

GameResult MakeResult(std::vector<int> scores) {
  GameResult result;
  result.winner_id = 0;
  result.scores = std::move(scores);
  return result;
}

TEST(RewardsTest, SoleWinner) {
  EXPECT_THAT(Rewards(MakeResult({-10, -5})), ElementsAre(0, 1, 0, 0));
  EXPECT_THAT(Rewards(MakeResult({-3, -20, -1, -40})),
              ElementsAre(0, 0, 1, 0));
}

TEST(RewardsTest, TiesSplitTheReward) {
  EXPECT_THAT(Rewards(MakeResult({-7, -7})), ElementsAre(0.5, 0.5, 0, 0));
  // Player 0 isn't in the tie, so it gets nothing.
  EXPECT_THAT(Rewards(MakeResult({-9, -4, -6, -4})),
              ElementsAre(0, 0.5, 0, 0.5));
  EXPECT_THAT(Rewards(MakeResult({15, -2, 15, 15})),
              ElementsAre(1.0 / 3, 0, 1.0 / 3, 1.0 / 3));
  EXPECT_THAT(Rewards(MakeResult({-89, -89, -89, -89})),
              ElementsAre(0.25, 0.25, 0.25, 0.25));
}

TEST(RewardsTest, NoMarginWeightIsPlainRewards) {
  EXPECT_THAT(Rewards(MakeResult({-9, -4, -6, -4}), 0, 10),
              Eq(Rewards(MakeResult({-9, -4, -6, -4}))));
}

TEST(RewardsTest, MarginRewards) {
  // Only the margin: 0.5 + 0.5 * tanh(+-5 / 10).
  const double ahead = 0.5 + 0.5 * std::tanh(0.5);
  EXPECT_THAT(Rewards(MakeResult({-10, -5}), 1, 10),
              ElementsAre(DoubleEq(1 - ahead), DoubleEq(ahead), 0, 0));
  // Half and half.
  EXPECT_THAT(Rewards(MakeResult({-10, -5}), 0.5, 10),
              ElementsAre(DoubleEq(0.5 * (1 - ahead)),
                          DoubleEq(0.5 + 0.5 * ahead), 0, 0));
  // A tie is 0.5 either way.
  EXPECT_THAT(Rewards(MakeResult({-7, -7}), 0.5, 10),
              ElementsAre(DoubleEq(0.5), DoubleEq(0.5), 0, 0));
  // With 4 players, each margin is over the best other player.
  EXPECT_THAT(Rewards(MakeResult({-3, -20, -1, -40}), 1, 10),
              ElementsAre(DoubleEq(0.5 + 0.5 * std::tanh(-0.2)),
                          DoubleEq(0.5 + 0.5 * std::tanh(-1.9)),
                          DoubleEq(0.5 + 0.5 * std::tanh(0.2)),
                          DoubleEq(0.5 + 0.5 * std::tanh(-3.9))));
}

TEST(RewardsTest, FromFinishedGame) {
  // Every color passes, so all 4 players tie.
  Game game(4);
  while (!game.Finished()) {
    ASSERT_TRUE(game.MakeMove(Move::EmptyMove(game.current_color())));
  }
  EXPECT_THAT(Rewards(game.Result()), ElementsAre(0.25, 0.25, 0.25, 0.25));
}

TEST(RolloutTest, RewardsSumToOne) {
  std::mt19937_64 rng(1);
  for (int i = 0; i < 20; ++i) {
    const PlayerRewards rewards2 = Rollout(Game(2), rng);
    EXPECT_THAT(rewards2[0] + rewards2[1], DoubleEq(1.0));
    EXPECT_THAT(rewards2[2] + rewards2[3], Eq(0));
    const PlayerRewards rewards4 = Rollout(Game(4), rng);
    EXPECT_THAT(rewards4[0] + rewards4[1] + rewards4[2] + rewards4[3],
                DoubleEq(1.0));
  }
}

TEST(RolloutTest, SameSeedSameRewards) {
  std::mt19937_64 rng1(7);
  std::mt19937_64 rng2(7);
  for (int i = 0; i < 10; ++i) {
    EXPECT_THAT(Rollout(Game(4), rng1), Eq(Rollout(Game(4), rng2)));
  }
}

TEST(RolloutTest, BiasedRolloutsFinish) {
  std::mt19937_64 rng(3);
  for (double size_bias : {-1.0, 1.0, 4.0}) {
    for (int i = 0; i < 5; ++i) {
      const PlayerRewards rewards =
          Rollout(Game(2), rng, {.size_bias = size_bias});
      EXPECT_THAT(rewards[0] + rewards[1], DoubleEq(1.0));
    }
  }
}

TEST(RolloutTest, BiasedSameSeedSameRewards) {
  std::mt19937_64 rng1(7);
  std::mt19937_64 rng2(7);
  for (int i = 0; i < 10; ++i) {
    const RolloutOptions options = {.size_bias = 1.5};
    EXPECT_THAT(Rollout(Game(4), rng1, options),
                Eq(Rollout(Game(4), rng2, options)));
  }
}

TEST(RolloutMoveTest, PassesWithoutMoves) {
  Game game(4);
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(BLUE)));
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(game.MakeMove(game.PossibleMove(0)));
  }
  ASSERT_THAT(game.current_color(), Eq(BLUE));
  std::mt19937_64 rng(1);
  EXPECT_THAT(RolloutMove(game, {}, rng), Eq(Move::EmptyMove(BLUE)));
  EXPECT_THAT(RolloutMove(game, {.size_bias = 0}, rng),
              Eq(Move::EmptyMove(BLUE)));
}

TEST(RolloutMoveTest, ZeroBiasPicksUniformly) {
  Game game(2);
  std::mt19937_64 rng1(5);
  std::mt19937_64 rng2(5);
  for (int i = 0; i < 100; ++i) {
    std::uniform_int_distribution<int> pick(0, game.NumPossibleMoves() - 1);
    EXPECT_THAT(RolloutMove(game, {.size_bias = 0}, rng1),
                Eq(game.PossibleMove(pick(rng2))));
  }
}

TEST(RolloutMoveTest, LargeBiasPicksLargestOrSmallestTiles) {
  Game game(2);
  std::mt19937_64 rng(2);
  for (int i = 0; i < 200; ++i) {
    EXPECT_THAT(kTiles[RolloutMove(game, {.size_bias = 20}, rng).tile].Size(),
                Eq(5));
    EXPECT_THAT(kTiles[RolloutMove(game, {.size_bias = -20}, rng).tile].Size(),
                Eq(1));
  }
}

TEST(RolloutMoveTest, BiasWeightsTilesBySize) {
  // After a few moves, so the counts aren't the opening's.
  Game game(2);
  for (int i = 0; i < 8; ++i) {
    ASSERT_TRUE(game.MakeMove(game.PossibleMove(game.NumPossibleMoves() / 2)));
  }
  const double size_bias = 0.5;
  // Each move has weight exp(size_bias * size), so the chance of a tile size
  // is the weighted share of the moves with that size.
  const std::array<int, kNumTiles> counts = game.NumPossibleMovesByTile();
  std::array<double, 6> expected = {};
  double total = 0;
  int num_pentomino_moves = 0;
  for (int tile = 0; tile < kNumTiles; ++tile) {
    const int size = kTiles[tile].Size();
    const double weight = counts[tile] * std::exp(size_bias * size);
    expected[size] += weight;
    total += weight;
    if (size == 5) num_pentomino_moves += counts[tile];
  }

  constexpr int kSamples = 20000;
  std::array<int, 6> seen = {};
  std::mt19937_64 rng(4);
  for (int i = 0; i < kSamples; ++i) {
    const Move move = RolloutMove(game, {.size_bias = size_bias}, rng);
    ++seen[kTiles[move.tile].Size()];
  }
  for (int size = 1; size <= 5; ++size) {
    EXPECT_THAT(static_cast<double>(seen[size]) / kSamples,
                DoubleNear(expected[size] / total, 0.015))
        << "size " << size;
  }
  // The bias makes pentominoes more common than uniform picks would.
  EXPECT_THAT(expected[5] / total,
              Gt(static_cast<double>(num_pentomino_moves) /
                 game.NumPossibleMoves()));
}

TEST(MctsAITest, OpeningMoveIsLegal) {
  Game game(4);
  MctsAI ai(0, SmallOptions(100, /*seed=*/1));
  const Move move = ai.SelectMove(game);
  EXPECT_THAT(move.color, Eq(BLUE));
  EXPECT_TRUE(game.MakeMove(move)) << move.DebugString();
}

// With fewer iterations than moves, the visited moves tie at one visit each.
// Ties must not go to the first move, which is always the smallest tile.
TEST(MctsAITest, FewIterationsDoNotFavorSmallTiles) {
  int num_pentominoes = 0;
  for (uint64_t seed = 0; seed < 40; ++seed) {
    Game game(2);
    MctsOptions options = SmallOptions(3, seed);
    // Uniform rollouts, so the rewards don't depend on tile sizes.
    options.rollout.size_bias = 0;
    MctsAI ai(0, options);
    num_pentominoes += kTiles[ai.SelectMove(game).tile].Size() == 5;
  }
  EXPECT_THAT(num_pentominoes, Gt(25));
}

// Each SelectMove replays the opponents' moves into the tree it kept from its
// previous move. These full games exercise that for both player counts.
TEST(MctsAITest, PlaysFullGame4Players) {
  EXPECT_THAT(PlayMctsGame(4, SmallOptions(10, /*seed=*/1)), SizeIs(Gt(40)));
}

TEST(MctsAITest, PlaysFullGame2Players) {
  EXPECT_THAT(PlayMctsGame(2, SmallOptions(10, /*seed=*/2)), SizeIs(Gt(40)));
}

TEST(MctsAITest, PlaysFullGameWithThreads) {
  EXPECT_THAT(
      PlayMctsGame(2, SmallOptions(20, /*seed=*/3, /*num_threads=*/4)),
      SizeIs(Gt(40)));
}

TEST(MctsAITest, PlaysFullGameWithVirtualLoss) {
  MctsOptions options = SmallOptions(20, /*seed=*/3, /*num_threads=*/4);
  options.virtual_loss = 3;
  EXPECT_THAT(PlayMctsGame(2, options), SizeIs(Gt(40)));
}

// A single thread doesn't use virtual loss, so it plays the same moves.
TEST(MctsAITest, VirtualLossDoesNotChangeSingleThread) {
  MctsOptions options = SmallOptions(20, /*seed=*/8);
  options.virtual_loss = 0;
  const std::vector<Move> without = PlayMctsGame(2, options);
  options.virtual_loss = 3;
  EXPECT_THAT(PlayMctsGame(2, options), Eq(without));
}

// With a seed and a single thread, a whole game is reproducible.
TEST(MctsAITest, SameSeedSameGame) {
  const std::vector<Move> game1 = PlayMctsGame(2, SmallOptions(10, 5));
  const std::vector<Move> game2 = PlayMctsGame(2, SmallOptions(10, 5));
  const std::vector<Move> other = PlayMctsGame(2, SmallOptions(10, 6));
  EXPECT_THAT(game1, Eq(game2));
  EXPECT_THAT(game1, Ne(other));
}

// An opponent may pass while it still has moves. That pass is not in the tree
// the AI kept from its previous move.
TEST(MctsAITest, OpponentPassesEarly) {
  Game game(2);
  // Enough iterations that the chosen opening move gets expanded.
  MctsAI ai(0, SmallOptions(200, /*seed=*/4));
  const Move blue = ai.SelectMove(game);
  ASSERT_TRUE(game.MakeMove(blue)) << blue.DebugString();
  ASSERT_THAT(game.PossibleMoves(), Not(IsEmpty()));
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(YELLOW)));
  const Move red = ai.SelectMove(game);
  EXPECT_THAT(red.color, Eq(RED));
  EXPECT_TRUE(game.MakeMove(red)) << red.DebugString();
}

}  // namespace
}  // namespace blokus
