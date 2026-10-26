#include "ai/mcts.h"

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

namespace blokus {
namespace {

using ::testing::DoubleEq;
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

TEST(MctsAITest, OpeningMoveIsLegal) {
  Game game(4);
  MctsAI ai(0, SmallOptions(100, /*seed=*/1));
  const Move move = ai.SelectMove(game);
  EXPECT_THAT(move.color, Eq(BLUE));
  EXPECT_TRUE(game.MakeMove(move)) << move.DebugString();
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
