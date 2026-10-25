#include "ai/mcts.h"

#include <cstdlib>
#include <memory>
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
using ::testing::Not;

MctsOptions SmallOptions(int num_iterations, int num_threads = 1) {
  MctsOptions options;
  options.num_iterations = num_iterations;
  options.num_threads = num_threads;
  return options;
}

// Plays a full game between MctsAIs, and returns the number of moves.
int PlayMctsGame(int num_players, const MctsOptions& options) {
  GameRunner runner(num_players);
  for (int id = 0; id < num_players; ++id) {
    runner.AddPlayer(std::make_unique<MctsAI>(id, options));
  }
  int num_moves = 0;
  runner.AddObserver([&num_moves](const Game&, const Move&) { ++num_moves; });
  runner.Play();
  return num_moves;
}

TEST(RolloutTest, ReturnsAPlayerId) {
  std::srand(1);
  for (int i = 0; i < 5; ++i) {
    const int winner2 = Rollout(Game(2));
    EXPECT_TRUE(winner2 == 0 || winner2 == 1) << winner2;
    const int winner4 = Rollout(Game(4));
    EXPECT_TRUE(winner4 >= 0 && winner4 < 4) << winner4;
  }
}

TEST(MctsAITest, OpeningMoveIsLegal) {
  std::srand(1);
  Game game(4);
  MctsAI ai(0, SmallOptions(100));
  const Move move = ai.SelectMove(game);
  EXPECT_THAT(move.color, Eq(BLUE));
  EXPECT_TRUE(game.MakeMove(move)) << move.DebugString();
}

// Each SelectMove replays the opponents' moves into the tree it kept from its
// previous move. These full games exercise that for both player counts.
TEST(MctsAITest, PlaysFullGame4Players) {
  std::srand(1);
  EXPECT_THAT(PlayMctsGame(4, SmallOptions(10)), Gt(40));
}

TEST(MctsAITest, PlaysFullGame2Players) {
  std::srand(2);
  EXPECT_THAT(PlayMctsGame(2, SmallOptions(10)), Gt(40));
}

TEST(MctsAITest, PlaysFullGameWithThreads) {
  std::srand(3);
  EXPECT_THAT(PlayMctsGame(2, SmallOptions(20, /*num_threads=*/4)), Gt(40));
}

// An opponent may pass while it still has moves. That pass is not in the tree
// the AI kept from its previous move.
TEST(MctsAITest, OpponentPassesEarly) {
  std::srand(4);
  Game game(2);
  // Enough iterations that the chosen opening move gets expanded.
  MctsAI ai(0, SmallOptions(200));
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
