#include "ai/mcts.h"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "game/game.h"
#include "game/game_runner.h"
#include "game/player.h"

namespace blokus {
namespace {

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

TEST(RolloutTest, ReturnsAPlayerId) {
  std::mt19937_64 rng(1);
  for (int i = 0; i < 5; ++i) {
    const int winner2 = Rollout(Game(2), rng);
    EXPECT_TRUE(winner2 == 0 || winner2 == 1) << winner2;
    const int winner4 = Rollout(Game(4), rng);
    EXPECT_TRUE(winner4 >= 0 && winner4 < 4) << winner4;
  }
}

TEST(RolloutTest, SameSeedSameWinners) {
  std::mt19937_64 rng1(7);
  std::mt19937_64 rng2(7);
  std::vector<int> winners1;
  std::vector<int> winners2;
  for (int i = 0; i < 10; ++i) {
    winners1.push_back(Rollout(Game(4), rng1));
    winners2.push_back(Rollout(Game(4), rng2));
  }
  EXPECT_THAT(winners1, Eq(winners2));
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
