#include "game/board.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace blokus {
namespace {

using ::testing::Eq;
using ::testing::SizeIs;

TEST(BoardTest, IsPossible) {
  Board board;
  // The 1x1 tile should be possible at the start in any orientation, but
  // not anywhere else on the board.
  for (int rotation = 0; rotation < 4; rotation++) {
    for (bool flip : {false, true}) {
      for (int8_t x = 0; x < Board::kWidth; ++x) {
        for (int8_t y = 0; y < Board::kHeight; ++y) {
          if (x == 0 && y == 0) {
            EXPECT_TRUE(board.IsPossible({BLUE, 0, {{x, y}, rotation, flip}}));
          } else {
            EXPECT_FALSE(board.IsPossible({BLUE, 0, {{x, y}, rotation, flip}}));
          }
        }
      }
    }
  }
}

TEST(BoardTest, PossibleMoves_Start_1x1) {
  Board board;
  std::vector<Move> moves = board.PossibleMoves(kTiles[0], BLUE);
  ASSERT_THAT(moves, SizeIs(1));
  EXPECT_THAT(moves[0].placement.coord, Eq(Coord(0, 0)));
  EXPECT_TRUE(board.IsPossible(moves[0]));

  moves = board.PossibleMoves(kTiles[0], YELLOW);
  ASSERT_THAT(moves, SizeIs(1));
  EXPECT_THAT(moves[0].placement.coord, Eq(Coord(0, Board::kWidth - 1)));
  EXPECT_TRUE(board.IsPossible(moves[0]));

  moves = board.PossibleMoves(kTiles[0], RED);
  ASSERT_THAT(moves, SizeIs(1));
  EXPECT_THAT(moves[0].placement.coord,
              Eq(Coord(Board::kHeight - 1, Board::kWidth - 1)));
  EXPECT_TRUE(board.IsPossible(moves[0]));

  moves = board.PossibleMoves(kTiles[0], GREEN);
  ASSERT_THAT(moves, SizeIs(1));
  EXPECT_THAT(moves[0].placement.coord, Eq(Coord(Board::kHeight - 1, 0)));
  EXPECT_TRUE(board.IsPossible(moves[0]));
}

TEST(BoardTest, PossibleMoves_Start_2x1) {
  Board board;
  std::vector<Move> moves = board.PossibleMoves(kTiles[1], BLUE);
  ASSERT_THAT(moves, SizeIs(2));
  EXPECT_THAT(moves[0].placement.coord, Eq(Coord(0, 0)));
  EXPECT_THAT(moves[0].placement.rotation, Eq(0));
  EXPECT_THAT(moves[0].placement.flip, Eq(false));
  EXPECT_TRUE(board.IsPossible(moves[0]));
  EXPECT_THAT(moves[1].placement.coord, Eq(Coord(1, 0)));
  EXPECT_THAT(moves[1].placement.rotation, Eq(1));
  EXPECT_THAT(moves[1].placement.flip, Eq(false));
  EXPECT_TRUE(board.IsPossible(moves[1]));

  moves = board.PossibleMoves(kTiles[1], YELLOW);
  ASSERT_THAT(moves, SizeIs(2));
  EXPECT_THAT(moves[0].placement.coord, Eq(Coord(0, Board::kWidth - 2)));
  EXPECT_THAT(moves[0].placement.rotation, Eq(0));
  EXPECT_THAT(moves[0].placement.flip, Eq(false));
  EXPECT_TRUE(board.IsPossible(moves[0]));
  EXPECT_THAT(moves[1].placement.coord, Eq(Coord(1, Board::kWidth - 1)));
  EXPECT_THAT(moves[1].placement.rotation, Eq(1));
  EXPECT_THAT(moves[1].placement.flip, Eq(false));
  EXPECT_TRUE(board.IsPossible(moves[1]));
}

TEST(BoardTest, PossibleMoves_Ladder) {
  Board board;
  std::vector<Move> moves = board.PossibleMoves(kTiles[16], BLUE);
  ASSERT_THAT(moves, SizeIs(2));
  EXPECT_THAT(moves[0].placement.coord, Eq(Coord(0, 0)));
  EXPECT_THAT(moves[0].placement.rotation, Eq(0));
  EXPECT_THAT(moves[0].placement.flip, Eq(false));
  EXPECT_TRUE(board.IsPossible(moves[0]));
  EXPECT_THAT(moves[1].placement.coord, Eq(Coord(0, 0)));
  EXPECT_THAT(moves[1].placement.rotation, Eq(1));
  EXPECT_THAT(moves[1].placement.flip, Eq(true));
  EXPECT_TRUE(board.IsPossible(moves[1]));

  moves = board.PossibleMoves(kTiles[16], YELLOW);
  ASSERT_THAT(moves, SizeIs(2));
  EXPECT_THAT(moves[0].placement.coord, Eq(Coord(2, Board::kWidth - 3)));
  EXPECT_THAT(moves[0].placement.rotation, Eq(0));
  EXPECT_THAT(moves[0].placement.flip, Eq(true));
  EXPECT_TRUE(board.IsPossible(moves[0]));
  EXPECT_THAT(moves[1].placement.coord, Eq(Coord(2, Board::kWidth - 3)));
  EXPECT_THAT(moves[1].placement.rotation, Eq(1));
  EXPECT_THAT(moves[1].placement.flip, Eq(false));
  EXPECT_TRUE(board.IsPossible(moves[1]));
}

// Test out possible moves from start played with "ladder" tile. After first
// move, upper left hand part of board is like:
//  ____
// |X
// |XX
// | XX
TEST(BoardTest, PossibleMoves_PlayedLadder) {
  Board board;
  Move move;
  move.color = BLUE;
  move.tile = 16;  // "ladder" tile.
  move.placement = {Coord(0, 0), 0, false};
  ASSERT_TRUE(board.MakeMove(move));

  // Simple case, just playing a 1x1 tile.
  std::vector<Move> moves = board.PossibleMoves(kTiles[0], BLUE);
  ASSERT_THAT(moves, SizeIs(4));
  EXPECT_THAT(moves[0].placement.coord, Eq(Coord(0, 2)));
  EXPECT_THAT(moves[1].placement.coord, Eq(Coord(1, 3)));
  EXPECT_THAT(moves[2].placement.coord, Eq(Coord(3, 0)));
  EXPECT_THAT(moves[3].placement.coord, Eq(Coord(3, 3)));
}

}  // namespace
}  // namespace blokus
