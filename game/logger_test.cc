#include "game/logger.h"

#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "game/game.h"
#include "game/game_log.pb.h"
#include "game/game_runner.h"
#include "game/player.h"

namespace blokus {
namespace {

using ::testing::Eq;
using ::testing::SizeIs;

// Plays the first possible move, or passes if there are none.
class FirstMovePlayer : public Player {
 public:
  using Player::Player;

  Move SelectMove(const Game& game) override {
    const std::vector<Move> moves = game.PossibleMoves();
    if (moves.empty()) return Move::EmptyMove(game.current_color());
    return moves.front();
  }
};

std::string TestPath(const std::string& name) {
  const char* dir = std::getenv("TEST_TMPDIR");
  return std::string(dir != nullptr ? dir : "/tmp") + "/" + name;
}

// Plays a 2-player game with `logger` attached, and returns the moves.
std::vector<Move> PlayLoggedGame(GameLogger& logger) {
  GameRunner runner(2);
  runner.AddPlayer(std::make_unique<FirstMovePlayer>(0));
  runner.AddPlayer(std::make_unique<FirstMovePlayer>(1));
  runner.AddObserver(logger.NewGame());
  std::vector<Move> moves;
  runner.AddObserver(
      [&moves](const Game&, const Move& move) { moves.push_back(move); });
  runner.Play();
  return moves;
}

void ExpectMovesMatch(const GameLog& log, const std::vector<Move>& moves) {
  ASSERT_THAT(log.moves(), SizeIs(moves.size()));
  for (int i = 0; i < static_cast<int>(moves.size()); ++i) {
    const GameLog::Move& proto = log.moves(i);
    const Move& move = moves[i];
    EXPECT_THAT(static_cast<int>(proto.color()), Eq(move.color)) << i;
    EXPECT_THAT(proto.tile(), Eq(move.tile)) << i;
    EXPECT_THAT(proto.placement().row(), Eq(move.placement.coord.row())) << i;
    EXPECT_THAT(proto.placement().col(), Eq(move.placement.coord.col())) << i;
    EXPECT_THAT(proto.placement().rotation(), Eq(move.placement.rotation))
        << i;
    EXPECT_THAT(proto.placement().flip(), Eq(move.placement.flip)) << i;
  }
}

TEST(GameLoggerTest, WritesAllGamesOnDestruction) {
  const std::string path = TestPath("games.pb");
  std::vector<Move> game1;
  std::vector<Move> game2;
  {
    GameLogger logger(path);
    game1 = PlayLoggedGame(logger);
    game2 = PlayLoggedGame(logger);
  }

  std::ifstream in(path, std::ios::binary);
  ASSERT_TRUE(in.is_open());
  GamesLog log;
  ASSERT_TRUE(log.ParseFromIstream(&in));
  ASSERT_THAT(log.games(), SizeIs(2));
  ExpectMovesMatch(log.games(0), game1);
  ExpectMovesMatch(log.games(1), game2);
}

TEST(GameLoggerTest, PathNeedNotBeNullTerminated) {
  const std::string path = TestPath("unterminated.pb");
  const std::string padded = path + "-extra";
  {
    GameLogger logger(std::string_view(padded).substr(0, path.size()));
  }
  std::ifstream in(path, std::ios::binary);
  EXPECT_TRUE(in.is_open());
}

TEST(GameLoggerDeathTest, BadPathDies) {
  EXPECT_DEATH(GameLogger("/nonexistent/dir/games.pb"), "is_open");
}

}  // namespace
}  // namespace blokus
