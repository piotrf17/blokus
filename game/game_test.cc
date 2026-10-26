#include "game/game.h"

#include <algorithm>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace blokus {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::Not;
using ::testing::SizeIs;

Move MakeTestMove(Color color, int tile, int row, int col, int rotation = 0,
                  bool flip = false) {
  Move move;
  move.color = color;
  move.tile = tile;
  move.placement = {Coord(row, col), rotation, flip};
  return move;
}

// Plays the 1x1 tile in each color's corner, in turn order.
void PlayOneInEachCorner(Game& game) {
  ASSERT_TRUE(game.MakeMove(MakeTestMove(BLUE, 0, 0, 0)));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(YELLOW, 0, 0, 19)));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(RED, 0, 19, 19)));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(GREEN, 0, 19, 0)));
}

// Makes every color pass until the game is finished.
void PassUntilFinished(Game& game) {
  while (!game.Finished()) {
    ASSERT_TRUE(game.MakeMove(Move::EmptyMove(game.current_color())));
  }
}

// Plays all 21 tiles for the current color, while all other colors pass.
// `tile_order` gives the order in which to play the tiles. Searches
// depth-first over placements, and returns false if no complete sequence is
// found.
bool PlayAllTilesAlone(Game& game, const std::vector<int>& tile_order,
                       int depth = 0) {
  if (depth == static_cast<int>(tile_order.size())) return true;
  const Color color = game.current_color();
  for (const Move& move : game.PossibleMoves()) {
    if (move.tile != tile_order[depth]) continue;
    Game next = game;
    if (!next.MakeMove(move)) return false;
    for (Color other = NextColor(color); other != color;
         other = NextColor(other)) {
      if (!next.MakeMove(Move::EmptyMove(other))) return false;
    }
    if (PlayAllTilesAlone(next, tile_order, depth + 1)) {
      game = next;
      return true;
    }
  }
  return false;
}

// Tile indices sorted from largest to smallest, so the 1x1 tile is last.
std::vector<int> LargestFirst() {
  std::vector<int> tiles;
  for (int tile = 0; tile < kNumTiles; ++tile) tiles.push_back(tile);
  std::stable_sort(tiles.begin(), tiles.end(), [](int a, int b) {
    return kTiles[a].Size() > kTiles[b].Size();
  });
  return tiles;
}

TEST(GameTest, InitialState) {
  Game game(4);
  EXPECT_THAT(game.num_players(), Eq(4));
  EXPECT_THAT(game.current_player(), Eq(0));
  EXPECT_THAT(game.current_color(), Eq(BLUE));
  EXPECT_FALSE(game.Finished());
  EXPECT_THAT(game.moves(), IsEmpty());
}

TEST(GameDeathTest, InvalidNumPlayersDies) {
  EXPECT_DEATH(Game(1), "num_players");
  EXPECT_DEATH(Game(3), "num_players");
}

TEST(GameTest, TurnOrder4Players) {
  Game game(4);
  EXPECT_THAT(game.current_color(), Eq(BLUE));
  EXPECT_THAT(game.current_player(), Eq(0));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(BLUE, 0, 0, 0)));
  EXPECT_THAT(game.current_color(), Eq(YELLOW));
  EXPECT_THAT(game.current_player(), Eq(1));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(YELLOW, 0, 0, 19)));
  EXPECT_THAT(game.current_color(), Eq(RED));
  EXPECT_THAT(game.current_player(), Eq(2));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(RED, 0, 19, 19)));
  EXPECT_THAT(game.current_color(), Eq(GREEN));
  EXPECT_THAT(game.current_player(), Eq(3));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(GREEN, 0, 19, 0)));
  EXPECT_THAT(game.current_color(), Eq(BLUE));
  EXPECT_THAT(game.current_player(), Eq(0));
}

TEST(GameTest, TurnOrder2Players) {
  // Player 0 plays BLUE and RED, player 1 plays YELLOW and GREEN.
  Game game(2);
  EXPECT_THAT(game.current_color(), Eq(BLUE));
  EXPECT_THAT(game.current_player(), Eq(0));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(BLUE, 0, 0, 0)));
  EXPECT_THAT(game.current_color(), Eq(YELLOW));
  EXPECT_THAT(game.current_player(), Eq(1));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(YELLOW, 0, 0, 19)));
  EXPECT_THAT(game.current_color(), Eq(RED));
  EXPECT_THAT(game.current_player(), Eq(0));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(RED, 0, 19, 19)));
  EXPECT_THAT(game.current_color(), Eq(GREEN));
  EXPECT_THAT(game.current_player(), Eq(1));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(GREEN, 0, 19, 0)));
  EXPECT_THAT(game.current_color(), Eq(BLUE));
  EXPECT_THAT(game.current_player(), Eq(0));
}

TEST(GameTest, MovesAreRecorded) {
  Game game(4);
  const Move blue = MakeTestMove(BLUE, 0, 0, 0);
  const Move yellow_pass = Move::EmptyMove(YELLOW);
  ASSERT_TRUE(game.MakeMove(blue));
  ASSERT_TRUE(game.MakeMove(yellow_pass));
  ASSERT_THAT(game.moves(), SizeIs(2));
  EXPECT_THAT(game.moves()[0].tile, Eq(0));
  EXPECT_THAT(game.moves()[0].placement, Eq(blue.placement));
  EXPECT_THAT(game.moves()[1].color, Eq(YELLOW));
  EXPECT_THAT(game.moves()[1].tile, Eq(-1));
}

TEST(GameTest, WrongColorIsRejected) {
  Game game(4);
  EXPECT_FALSE(game.MakeMove(MakeTestMove(YELLOW, 0, 0, 19)));
  EXPECT_FALSE(game.MakeMove(Move::EmptyMove(YELLOW)));
  EXPECT_THAT(game.current_color(), Eq(BLUE));
  EXPECT_THAT(game.moves(), IsEmpty());
}

TEST(GameTest, IllegalPlacementIsRejected) {
  Game game(4);
  // BLUE must start in its own corner.
  EXPECT_FALSE(game.MakeMove(MakeTestMove(BLUE, 0, 0, 19)));
  EXPECT_FALSE(game.MakeMove(MakeTestMove(BLUE, 0, 5, 5)));
  EXPECT_THAT(game.current_color(), Eq(BLUE));
  EXPECT_THAT(game.moves(), IsEmpty());
  // The tile is still available.
  EXPECT_TRUE(game.MakeMove(MakeTestMove(BLUE, 0, 0, 0)));
}

TEST(GameTest, TileCanOnlyBePlayedOnce) {
  Game game(4);
  PlayOneInEachCorner(game);
  // (1, 1) would be a legal spot for the 1x1 tile, but it's been used.
  EXPECT_FALSE(game.MakeMove(MakeTestMove(BLUE, 0, 1, 1)));
  EXPECT_THAT(game.current_color(), Eq(BLUE));
  for (const Move& move : game.PossibleMoves()) {
    EXPECT_THAT(move.tile, Ne(0)) << move.DebugString();
  }
  // Another tile can go there.
  EXPECT_TRUE(game.MakeMove(MakeTestMove(BLUE, 1, 1, 1)));
}

TEST(GameTest, PossibleMovesAtStart) {
  Game game(4);
  const std::vector<Move> moves = game.PossibleMoves();
  // Every move is for BLUE, and covers BLUE's corner.
  for (const Move& move : moves) {
    EXPECT_THAT(move.color, Eq(BLUE));
    EXPECT_TRUE(game.board().IsPossible(move)) << move.DebugString();
  }
  // The 1x1 tile fits one way, and the 2x1 tile two ways.
  EXPECT_THAT(std::count_if(moves.begin(), moves.end(),
                            [](const Move& m) { return m.tile == 0; }),
              Eq(1));
  EXPECT_THAT(std::count_if(moves.begin(), moves.end(),
                            [](const Move& m) { return m.tile == 1; }),
              Eq(2));
  // Every tile has an opening move, except the cross (tile 19), which has no
  // cell in the corner of its bounding box.
  for (int tile = 0; tile < kNumTiles; ++tile) {
    EXPECT_THAT(std::any_of(moves.begin(), moves.end(),
                            [tile](const Move& m) { return m.tile == tile; }),
                Eq(tile != 19))
        << "tile " << tile;
  }
}

TEST(GameTest, PossibleMovesAreForCurrentColor) {
  Game game(4);
  ASSERT_TRUE(game.MakeMove(MakeTestMove(BLUE, 0, 0, 0)));
  const std::vector<Move> moves = game.PossibleMoves();
  ASSERT_THAT(moves, Not(IsEmpty()));
  for (const Move& move : moves) {
    EXPECT_THAT(move.color, Eq(YELLOW));
  }
}

TEST(GameTest, PassingIsPermanent) {
  Game game(4);
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(BLUE)));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(YELLOW, 0, 0, 19)));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(RED, 0, 19, 19)));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(GREEN, 0, 19, 0)));
  // BLUE passed, so it can't play a tile any more, only pass again.
  EXPECT_FALSE(game.MakeMove(MakeTestMove(BLUE, 0, 0, 0)));
  EXPECT_THAT(game.current_color(), Eq(BLUE));
  EXPECT_TRUE(game.MakeMove(Move::EmptyMove(BLUE)));
  EXPECT_THAT(game.current_color(), Eq(YELLOW));
  EXPECT_FALSE(game.Finished());
}

TEST(GameTest, NoPossibleMovesAfterPassing) {
  Game game(4);
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(BLUE)));
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(YELLOW)));
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(RED)));
  EXPECT_THAT(game.PossibleMoves(), Not(IsEmpty()));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(GREEN, 0, 19, 0)));
  // BLUE still has legal placements, but it already passed.
  EXPECT_THAT(game.current_color(), Eq(BLUE));
  EXPECT_THAT(game.PossibleMoves(), IsEmpty());
  EXPECT_THAT(game.NumPossibleMoves(), Eq(0));
}

TEST(GameTest, PossibleMoveMatchesPossibleMoves) {
  Game game(4);
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(BLUE)));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(YELLOW, 0, 0, 19)));
  // Play the first possible move each turn, so BLUE stays passed.
  while (!game.Finished()) {
    const std::vector<Move> moves = game.PossibleMoves();
    ASSERT_THAT(game.NumPossibleMoves(), Eq(static_cast<int>(moves.size())));
    for (int i = 0; i < static_cast<int>(moves.size()); ++i) {
      ASSERT_THAT(game.PossibleMove(i), Eq(moves[i])) << i;
    }
    ASSERT_TRUE(game.MakeMove(moves.empty()
                                  ? Move::EmptyMove(game.current_color())
                                  : moves.front()));
  }
}

TEST(GameTest, FinishedOnceEveryColorPasses) {
  Game game(4);
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(BLUE)));
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(YELLOW)));
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(RED)));
  EXPECT_FALSE(game.Finished());
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(GREEN)));
  EXPECT_TRUE(game.Finished());
}

TEST(GameTest, ScoreWithNoTilesPlayed) {
  Game game(4);
  PassUntilFinished(game);
  const GameResult result = game.Result();
  EXPECT_THAT(result.scores, ElementsAre(-89, -89, -89, -89));
}

TEST(GameTest, ScoreCountsUnplayedCells) {
  Game game(4);
  // BLUE plays the 1x1 (1 cell), YELLOW the 2x1 (2 cells), RED the 1x5
  // (5 cells), and GREEN passes.
  ASSERT_TRUE(game.MakeMove(MakeTestMove(BLUE, 0, 0, 0)));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(YELLOW, 1, 0, 18)));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(RED, 14, 19, 15)));
  PassUntilFinished(game);
  const GameResult result = game.Result();
  EXPECT_THAT(result.scores, ElementsAre(-88, -87, -84, -89));
  EXPECT_THAT(result.winner_id, Eq(2));
}

TEST(GameTest, ScoreSumsColorsFor2Players) {
  Game game(2);
  // Player 0 (BLUE + RED) plays 1 + 5 cells, player 1 (YELLOW + GREEN) plays
  // 2 + 0 cells.
  ASSERT_TRUE(game.MakeMove(MakeTestMove(BLUE, 0, 0, 0)));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(YELLOW, 1, 0, 18)));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(RED, 14, 19, 15)));
  PassUntilFinished(game);
  const GameResult result = game.Result();
  EXPECT_THAT(result.scores, ElementsAre(-88 - 84, -87 - 89));
  EXPECT_THAT(result.winner_id, Eq(0));
}

TEST(GameTest, TiedScoresGoToLowestPlayerId) {
  Game game(4);
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(BLUE)));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(YELLOW, 0, 0, 19)));
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(RED)));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(GREEN, 0, 19, 0)));
  PassUntilFinished(game);
  const GameResult result = game.Result();
  EXPECT_THAT(result.scores, ElementsAre(-89, -88, -89, -88));
  EXPECT_THAT(result.winner_id, Eq(1));
}

TEST(GameTest, BonusForPlayingAllTilesWithOneLast) {
  Game game(4);
  ASSERT_TRUE(PlayAllTilesAlone(game, LargestFirst()));
  ASSERT_THAT(game.moves().back().color, Eq(GREEN));
  PassUntilFinished(game);
  const GameResult result = game.Result();
  EXPECT_THAT(result.scores, ElementsAre(20, -89, -89, -89));
  EXPECT_THAT(result.winner_id, Eq(0));
}

TEST(GameTest, BonusForPlayingAllTiles) {
  Game game(4);
  // Same as above, but the 1x1 tile goes first.
  std::vector<int> tile_order = LargestFirst();
  std::rotate(tile_order.rbegin(), tile_order.rbegin() + 1,
              tile_order.rend());
  ASSERT_THAT(tile_order.front(), Eq(0));
  ASSERT_TRUE(PlayAllTilesAlone(game, tile_order));
  PassUntilFinished(game);
  const GameResult result = game.Result();
  EXPECT_THAT(result.scores, ElementsAre(15, -89, -89, -89));
}

TEST(GameTest, BonusSumsFor2Players) {
  Game game(2);
  ASSERT_TRUE(PlayAllTilesAlone(game, LargestFirst()));
  PassUntilFinished(game);
  const GameResult result = game.Result();
  // BLUE gets the bonus, but RED played nothing.
  EXPECT_THAT(result.scores, ElementsAre(20 - 89, -89 - 89));
}

}  // namespace
}  // namespace blokus
