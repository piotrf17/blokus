#include "ai/random.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "game/game.h"
#include "game/game_runner.h"

namespace blokus {
namespace {

using ::testing::Eq;
using ::testing::Gt;
using ::testing::Ne;
using ::testing::SizeIs;

// Plays a full game between RandomAIs, and returns the moves. Player i is
// seeded with `seed + i`.
std::vector<Move> PlayRandomGame(int num_players, uint64_t seed) {
  GameRunner runner(num_players);
  for (int id = 0; id < num_players; ++id) {
    runner.AddPlayer(std::make_unique<RandomAI>(id, seed + id));
  }
  std::vector<Move> moves;
  runner.AddObserver(
      [&moves](const Game&, const Move& move) { moves.push_back(move); });
  runner.Play();
  return moves;
}

TEST(RandomAITest, PlaysFullGames) {
  EXPECT_THAT(PlayRandomGame(4, 1), SizeIs(Gt(40)));
  EXPECT_THAT(PlayRandomGame(2, 2), SizeIs(Gt(40)));
}

TEST(RandomAITest, PassesWithNoMoves) {
  Game game(4);
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(BLUE)));
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(YELLOW)));
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(RED)));
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(GREEN)));
  // BLUE has already passed, so it has no moves.
  RandomAI ai(0, 1);
  EXPECT_THAT(ai.SelectMove(game).tile, Eq(-1));
}

TEST(RandomAITest, SameSeedSameGame) {
  EXPECT_THAT(PlayRandomGame(4, 5), Eq(PlayRandomGame(4, 5)));
  EXPECT_THAT(PlayRandomGame(4, 5), Ne(PlayRandomGame(4, 6)));
}

}  // namespace
}  // namespace blokus
