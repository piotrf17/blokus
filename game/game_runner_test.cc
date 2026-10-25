#include "game/game_runner.h"

#include <memory>
#include <vector>

#include "game/game.h"
#include "game/player.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace blokus {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

// Plays the first possible move, or passes if there are none. Records the
// color it was asked to play each time.
class FirstMovePlayer : public Player {
 public:
  explicit FirstMovePlayer(int player_id, std::vector<Color>* colors = nullptr)
      : Player(player_id), colors_(colors) {}

  Move SelectMove(const Game& game) override {
    if (colors_ != nullptr) colors_->push_back(game.current_color());
    const std::vector<Move> moves = game.PossibleMoves();
    if (moves.empty()) return Move::EmptyMove(game.current_color());
    return moves.front();
  }

 private:
  std::vector<Color>* colors_;
};

// Always passes. Sets `*destroyed` when it is destroyed.
class PassingPlayer : public Player {
 public:
  explicit PassingPlayer(int player_id, bool* destroyed = nullptr)
      : Player(player_id), destroyed_(destroyed) {}
  ~PassingPlayer() override {
    if (destroyed_ != nullptr) *destroyed_ = true;
  }

  Move SelectMove(const Game& game) override {
    return Move::EmptyMove(game.current_color());
  }

 private:
  bool* destroyed_;
};

// Tries to play the 1x1 tile in the middle of the board, which is illegal.
class IllegalPlayer : public Player {
 public:
  using Player::Player;

  Move SelectMove(const Game& game) override {
    Move move;
    move.color = game.current_color();
    move.tile = 0;
    move.placement = {Coord(10, 10), 0, false};
    return move;
  }
};

TEST(GameRunnerTest, AllPassing) {
  GameRunner runner(4);
  for (int id = 0; id < 4; ++id) {
    runner.AddPlayer(std::make_unique<PassingPlayer>(id));
  }
  const GameResult result = runner.Play();
  EXPECT_THAT(result.scores, ElementsAre(-89, -89, -89, -89));
  EXPECT_THAT(result.winner_id, Eq(0));
}

TEST(GameRunnerTest, PlayersAreAskedForTheirColors4Players) {
  std::vector<std::vector<Color>> colors(4);
  GameRunner runner(4);
  for (int id = 0; id < 4; ++id) {
    runner.AddPlayer(std::make_unique<FirstMovePlayer>(id, &colors[id]));
  }
  runner.Play();
  const Color expected[] = {BLUE, YELLOW, RED, GREEN};
  for (int id = 0; id < 4; ++id) {
    EXPECT_THAT(colors[id], Not(IsEmpty()));
    for (Color color : colors[id]) {
      EXPECT_THAT(color, Eq(expected[id])) << "player " << id;
    }
  }
}

TEST(GameRunnerTest, PlayersAreAskedForTheirColors2Players) {
  std::vector<Color> colors0;
  std::vector<Color> colors1;
  GameRunner runner(2);
  runner.AddPlayer(std::make_unique<FirstMovePlayer>(0, &colors0));
  runner.AddPlayer(std::make_unique<FirstMovePlayer>(1, &colors1));
  runner.Play();
  ASSERT_THAT(colors0, SizeIs(Gt(4)));
  ASSERT_THAT(colors1, SizeIs(Gt(4)));
  // Player 0 alternates BLUE and RED, player 1 YELLOW and GREEN.
  for (int i = 0; i < static_cast<int>(colors0.size()); ++i) {
    EXPECT_THAT(colors0[i], Eq(i % 2 == 0 ? BLUE : RED)) << i;
  }
  for (int i = 0; i < static_cast<int>(colors1.size()); ++i) {
    EXPECT_THAT(colors1[i], Eq(i % 2 == 0 ? YELLOW : GREEN)) << i;
  }
}

TEST(GameRunnerTest, ObserversSeeEveryMove) {
  GameRunner runner(4);
  for (int id = 0; id < 4; ++id) {
    runner.AddPlayer(std::make_unique<FirstMovePlayer>(id));
  }
  int calls = 0;
  std::vector<Move> moves;
  std::unique_ptr<Game> last_game;
  runner.AddObserver([&](const Game& game, const Move& move) {
    ++calls;
    moves.push_back(move);
    // The observer is called after the move is made.
    ASSERT_THAT(game.moves(), SizeIs(calls));
    EXPECT_THAT(game.moves().back().placement, Eq(move.placement));
    last_game = std::make_unique<Game>(game);
  });
  int second_calls = 0;
  runner.AddObserver([&](const Game&, const Move&) { ++second_calls; });

  const GameResult result = runner.Play();

  ASSERT_NE(last_game, nullptr);
  EXPECT_TRUE(last_game->Finished());
  EXPECT_THAT(calls, Eq(static_cast<int>(last_game->moves().size())));
  EXPECT_THAT(second_calls, Eq(calls));
  EXPECT_THAT(moves.front().color, Eq(BLUE));
  EXPECT_THAT(moves.front().tile, Eq(0));
  const GameResult expected = last_game->Result();
  EXPECT_THAT(result.scores, Eq(expected.scores));
  EXPECT_THAT(result.winner_id, Eq(expected.winner_id));
}

TEST(GameRunnerTest, PlayersAreDestroyedWithRunner) {
  bool destroyed = false;
  {
    GameRunner runner(2);
    runner.AddPlayer(std::make_unique<PassingPlayer>(0, &destroyed));
    runner.AddPlayer(std::make_unique<PassingPlayer>(1));
  }
  EXPECT_TRUE(destroyed);
}

TEST(GameRunnerDeathTest, PlayersMustBeAddedInIdOrder) {
  GameRunner runner(2);
  EXPECT_DEATH(runner.AddPlayer(std::make_unique<PassingPlayer>(1)),
               "expected player with id 0");
}

TEST(GameRunnerDeathTest, TooManyPlayers) {
  GameRunner runner(2);
  runner.AddPlayer(std::make_unique<PassingPlayer>(0));
  runner.AddPlayer(std::make_unique<PassingPlayer>(1));
  EXPECT_DEATH(runner.AddPlayer(std::make_unique<PassingPlayer>(2)),
               "Added too many players");
}

TEST(GameRunnerDeathTest, TooFewPlayers) {
  GameRunner runner(2);
  runner.AddPlayer(std::make_unique<PassingPlayer>(0));
  EXPECT_DEATH(runner.Play(), "Not enough players");
}

TEST(GameRunnerDeathTest, InvalidMoveDies) {
  GameRunner runner(2);
  runner.AddPlayer(std::make_unique<IllegalPlayer>(0));
  runner.AddPlayer(std::make_unique<PassingPlayer>(1));
  EXPECT_DEATH(runner.Play(), "blue \\(player 0\\) wants to play an invalid");
}

}  // namespace
}  // namespace blokus
