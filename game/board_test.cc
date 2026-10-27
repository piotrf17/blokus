#include "game/board.h"

#include <algorithm>
#include <map>
#include <random>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#define ANSI_COLOR_RESET   "\x1b[0m"

namespace blokus {
namespace {

using ::testing::ContainerEq;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

std::vector<Coord> PlacedTile(const Tile& tile, const Placement& placement) {
  std::vector<Coord> coors = tile.Transform(placement.rotation, placement.flip);
  for (auto& coor : coors) {
    coor[0] += placement.coord[0];
    coor[1] += placement.coord[1];
  }
  return coors;
}

// Returns a bit flag for `color`, or 0 for INVALID (an empty cell).
uint8_t ColorToByte(Color color) {
  if (color == INVALID) return 0;
  return 1 << (color - 1);
}

// This class is very similar to Board, but uses the older and slower algorithms
// for determining possible moves. We use it here to help test if the current
// Board is correct.
class VerificationBoard {
 public:
  static const int kNumRows = 20;
  static const int kNumCols = 20;

  VerificationBoard();
  ~VerificationBoard() {}

  bool IsPossible(const Move& move) const;
  std::vector<Move> PossibleMoves(const Tile& tile, Color color) const;
  bool MakeMove(const Move& move);
  // Debugging aid, prints the board plus the frontier and allowed maps.
  [[maybe_unused]] void Print(bool debug = false) const;

 private:
  void PrintAllowable(const uint8_t board[kNumRows][kNumCols]) const;
  
  Color pieces_[kNumRows][kNumCols];
  // Frontier is the outer border of our movements. Every move must contain
  // a bit of frontier.
  uint8_t frontier_[kNumRows][kNumCols];
  uint8_t allowed_[kNumRows][kNumCols];
};

VerificationBoard::VerificationBoard() {
  // Initially, there are no pieces on the board.
  for (int r = 0; r < kNumRows; ++r) {
    for (int c = 0; c < kNumCols; ++c) {
      pieces_[r][c] = INVALID;
    }
  }

  // Initially, you can only move in a corner. Place in turn order,
  // going clockwise from top-left (0,0).
  memset(frontier_, 0, kNumRows * kNumCols * sizeof(uint8_t));
  frontier_[0][0] = ColorToByte(BLUE);
  frontier_[0][kNumCols - 1] = ColorToByte(YELLOW);
  frontier_[kNumRows - 1][kNumRows - 1] = ColorToByte(RED);
  frontier_[kNumRows - 1][0] = ColorToByte(GREEN);

  // Initially, everyone is allowed to move everywhere.
  memset(allowed_,
         ColorToByte(BLUE) | ColorToByte(YELLOW) |
         ColorToByte(RED) | ColorToByte(GREEN),
         kNumRows * kNumCols * sizeof(uint8_t));
}

bool VerificationBoard::IsPossible(const Move& move) const {
  bool contains_frontier = false;
  for (const Coord& coord : PlacedTile(kTiles[move.tile], move.placement)) {
    const int r = coord.row();
    const int c = coord.col();

    // Verify that the piece is on the board.
    if (r < 0 || r > kNumRows - 1) {
      // TODO(piotrf): re-enable once absl has vlog
      //  VLOG(1) << " rows go off board!";
      return false;
    }
    if (c < 0 || c > kNumCols - 1) {
      // TODO(piotrf): re-enable once absl has vlog
      //  VLOG(1) << " cols go off board!";
      return false;
    }

    // Verify that we are allowed to move here.
    if ((allowed_[r][c] & ColorToByte(move.color)) == 0) {
      // TODO(piotrf): re-enable once absl has vlog
      //  VLOG(1) << coord << " is not allowed!";
      return false;
    }

    // Keep track of whether or not we touch the frontier. We must touch in at
    // least on place.
    contains_frontier |= frontier_[r][c] & ColorToByte(move.color);
  }

  if (!contains_frontier) {
    // TODO(piotrf): re-enable once absl has vlog
    //  VLOG(1) << "doesn't contain frontier!";
  }
  
  return contains_frontier;
}

std::vector<Move> VerificationBoard::PossibleMoves(
    const Tile& tile, Color color) const {
  std::vector<Move> moves;

  // Naive, brute-force algorithm: try all possible places on the board, with
  // all rotations and flips.
  for (int r = 0; r < kNumRows; ++r) {
    for (int c = 0; c < kNumCols; ++c) {
      for (const TileOrientation& orientation : tile.orientations()) {
        Move move;
        move.tile = tile.index();
        move.color = color;
        move.placement.coord = Coord(r, c);
        move.placement.rotation = orientation.rotation();
        move.placement.flip = orientation.flip();
        if (IsPossible(move)) {
          moves.push_back(std::move(move));
        }
      }
    }
  }

  return moves;
}
  
bool VerificationBoard::MakeMove(const Move& move) {
  if (!IsPossible(move)) {
    return false;
  }
  std::vector<Coord> coords = PlacedTile(kTiles[move.tile], move.placement);
  for (const Coord& coord : coords) {
    pieces_[coord.row()][coord.col()] = move.color;
  }
  // Update frontier and allowed based on current state of pieces_.
  for (int r = 0; r < kNumRows; ++r) {
    for (int c = 0; c < kNumCols; ++c) {
      // If there is a piece here, it's not on the frontier or allowed.
      if (pieces_[r][c]) {
        frontier_[r][c] = 0;
        allowed_[r][c] = 0;
        continue;
      }
      // If there is a piece horizontally or vertically separated, then
      // that color can't move here.
      uint8_t allowed = ColorToByte(BLUE) | ColorToByte(YELLOW) |
          ColorToByte(RED) | ColorToByte(GREEN);
      if (r > 0 && ColorToByte(pieces_[r - 1][c])) {
        allowed &= ~ColorToByte(pieces_[r - 1][c]);
      }
      if (r < kNumRows - 1 && ColorToByte(pieces_[r + 1][c])) {
        allowed &= ~ColorToByte(pieces_[r + 1][c]);
      }
      if (c > 0 && ColorToByte(pieces_[r][c - 1])) {
        allowed &= ~ColorToByte(pieces_[r][c - 1]);
      }
      if (c < kNumCols - 1 && ColorToByte(pieces_[r][c + 1])) {
        allowed &= ~ColorToByte(pieces_[r][c + 1]);
      }
      allowed_[r][c] = allowed;
      // If there is a piece corner separated from us, and this is an allowed
      // spot, then this is on our frontier.
      uint8_t frontier = 0;
      if (r > 0 && c > 0 && ColorToByte(pieces_[r - 1][c - 1])) {
        frontier |= ColorToByte(pieces_[r - 1][c - 1]);
      }
      if (r > 0 && c < kNumCols - 1 && ColorToByte(pieces_[r - 1][c + 1])) {
        frontier |= ColorToByte(pieces_[r - 1][c + 1]);
      }
      if (r < kNumRows - 1 && c > 0 && ColorToByte(pieces_[r + 1][c - 1])) {
        frontier |= ColorToByte(pieces_[r + 1][c - 1]);
      }
      if (r < kNumRows - 1 && c < kNumCols - 1 &&
          ColorToByte(pieces_[r + 1][c + 1])) {
        frontier |= ColorToByte(pieces_[r + 1][c + 1]);
      }
      frontier_[r][c] = frontier & allowed;
    }
  }
  // Always enforce initial frontier.
  // TODO(piotrf): clean up code duplication.
  frontier_[0][0] = ColorToByte(BLUE);
  frontier_[0][kNumCols - 1] = ColorToByte(YELLOW);
  frontier_[kNumRows - 1][kNumCols - 1] = ColorToByte(RED);
  frontier_[kNumRows - 1][0] = ColorToByte(GREEN);
  
  return true;  
}

void VerificationBoard::PrintAllowable(
    const uint8_t board[kNumRows][kNumCols]) const {
  printf("  ");
  for (int c = 0; c < kNumCols; ++c) {
    printf("  %2d ", c);
  }
  printf("\n");
  for (int r = 0; r < kNumRows; ++r) {
    printf("%2d ", r);
    for (int c = 0; c < kNumCols; ++c) {
      std::string out;
      for (Color color : {BLUE, YELLOW, RED, GREEN}) {
        if (board[r][c] & ColorToByte(color))
          out += AnsiColor(color) + "\u25a3";
        else
          out +=  " ";
      }
      out += ANSI_COLOR_RESET " ";
      printf("%s", out.c_str());
    }
    printf("\n");
  }
}

void VerificationBoard::Print(bool debug) const {
  // Print out the pieces on the board.
  printf("  ");
  for (int c = 0; c < kNumCols; ++c) {
    printf(" %2d", c);
  }
  printf("\n");
  for (int r = 0; r < kNumRows; ++r) {
    printf("%2d ", r);
    for (int c = 0; c < kNumCols; ++c) {
      if (pieces_[r][c]) {
        std::string out = AnsiColor(pieces_[r][c]) + "\u25a3";
        printf(" %s" ANSI_COLOR_RESET " ", out.c_str());
      } else {
        printf(" _ ");
      }
    }
    printf("\n");
  }

  if (debug) {
    printf("========== FRONTIER ==========\n");
    PrintAllowable(frontier_);
    printf("========== ALLOWED ==========\n");
    PrintAllowable(allowed_);
  }
}

Move MakeTestMove(Color color, int tile, int row, int col, int rotation = 0,
                  bool flip = false) {
  Move move;
  move.color = color;
  move.tile = tile;
  move.placement = {Coord(row, col), rotation, flip};
  return move;
}

TEST(BoardTest, IsPossible_Start_1x1) {
  Board board;
  // At the start, the 1x1 tile can only go in each color's own corner.
  for (int row = 0; row < Board::kNumRows; ++row) {
    for (int col = 0; col < Board::kNumCols; ++col) {
      EXPECT_THAT(board.IsPossible(MakeTestMove(BLUE, 0, row, col)),
                  Eq(row == 0 && col == 0));
      EXPECT_THAT(board.IsPossible(MakeTestMove(YELLOW, 0, row, col)),
                  Eq(row == 0 && col == Board::kNumCols - 1));
      EXPECT_THAT(board.IsPossible(MakeTestMove(RED, 0, row, col)),
                  Eq(row == Board::kNumRows - 1 && col == Board::kNumCols - 1));
      EXPECT_THAT(board.IsPossible(MakeTestMove(GREEN, 0, row, col)),
                  Eq(row == Board::kNumRows - 1 && col == 0));
    }
  }
}

TEST(BoardTest, IsPossible_OffBoard) {
  Board board;
  // The 1x5 tile, horizontal, starting in BLUE's corner.
  EXPECT_TRUE(board.IsPossible(MakeTestMove(BLUE, 14, 0, 0)));
  // Hanging off the left or top edge.
  EXPECT_FALSE(board.IsPossible(MakeTestMove(BLUE, 14, 0, -1)));
  EXPECT_FALSE(board.IsPossible(MakeTestMove(BLUE, 14, -1, 0)));
  // Vertical (rotation 1 goes up from the origin), so the origin must be at
  // row 4 for the tile to cover (0, 0).
  EXPECT_TRUE(board.IsPossible(MakeTestMove(BLUE, 14, 4, 0, 1)));
  EXPECT_FALSE(board.IsPossible(MakeTestMove(BLUE, 14, 3, 0, 1)));
}

TEST(BoardTest, MakeMove_SameColorRules) {
  Board board;
  // BLUE plays the 2x2 square in its corner, covering (0,0)-(1,1).
  ASSERT_TRUE(board.MakeMove(MakeTestMove(BLUE, 4, 0, 0)));
  // BLUE can't play on top of itself.
  EXPECT_FALSE(board.IsPossible(MakeTestMove(BLUE, 0, 1, 1)));
  // BLUE can't play edge-adjacent to itself, even if it also touches a corner.
  EXPECT_FALSE(board.IsPossible(MakeTestMove(BLUE, 0, 0, 2)));
  EXPECT_FALSE(board.IsPossible(MakeTestMove(BLUE, 0, 2, 1)));
  EXPECT_FALSE(board.IsPossible(MakeTestMove(BLUE, 1, 2, 1)));
  // BLUE can play touching only at a corner.
  EXPECT_TRUE(board.IsPossible(MakeTestMove(BLUE, 0, 2, 2)));
  // ... but not somewhere that touches nothing.
  EXPECT_FALSE(board.IsPossible(MakeTestMove(BLUE, 0, 3, 3)));
}

TEST(BoardTest, MakeMove_OtherColorsMayTouchEdges) {
  Board board;
  // BLUE zig-zags across the top with the 1x5: (0,0)-(0,4), (1,5)-(1,9),
  // then (0,10)-(0,14).
  ASSERT_TRUE(board.MakeMove(MakeTestMove(BLUE, 14, 0, 0)));
  ASSERT_TRUE(board.MakeMove(MakeTestMove(BLUE, 14, 1, 5)));
  ASSERT_TRUE(board.MakeMove(MakeTestMove(BLUE, 14, 0, 10)));
  // YELLOW's 1x5 in its corner, (0,15)-(0,19), is edge-adjacent to BLUE's
  // (0,14). That's allowed, since only same-color edges are forbidden.
  ASSERT_TRUE(board.MakeMove(MakeTestMove(YELLOW, 14, 0, 15)));
  // YELLOW's (1,14) is diagonal to YELLOW and edge-adjacent to BLUE: allowed.
  EXPECT_TRUE(board.IsPossible(MakeTestMove(YELLOW, 0, 1, 14)));
  // No color can play on top of another.
  EXPECT_FALSE(board.IsPossible(MakeTestMove(YELLOW, 0, 0, 14)));
  EXPECT_FALSE(board.IsPossible(MakeTestMove(BLUE, 0, 0, 15)));
}

TEST(BoardTest, MakeMove_InvalidMoveDoesNotChangeBoard) {
  Board board;
  const std::vector<Move> before = board.PossibleMoves(kTiles[4], BLUE);
  // Not touching BLUE's corner.
  EXPECT_FALSE(board.MakeMove(MakeTestMove(BLUE, 4, 5, 5)));
  EXPECT_THAT(board.PossibleMoves(kTiles[4], BLUE), ContainerEq(before));
  EXPECT_THAT(board.PossibleMoves(kTiles[4], YELLOW), SizeIs(1));
}

TEST(BoardTest, MoveCells) {
  // The 1x5 is horizontal at rotation 0 and goes up from the origin at
  // rotation 1.
  EXPECT_THAT(MakeTestMove(BLUE, 14, 0, 0).Cells(),
              UnorderedElementsAre(Coord(0, 0), Coord(0, 1), Coord(0, 2),
                                   Coord(0, 3), Coord(0, 4)));
  EXPECT_THAT(MakeTestMove(BLUE, 14, 4, 0, 1).Cells(),
              UnorderedElementsAre(Coord(0, 0), Coord(1, 0), Coord(2, 0),
                                   Coord(3, 0), Coord(4, 0)));
  EXPECT_THAT(Move::EmptyMove(BLUE).Cells(), IsEmpty());
}

TEST(BoardTest, AtShowsPlacedCells) {
  Board board;
  ASSERT_TRUE(board.MakeMove(MakeTestMove(BLUE, 14, 0, 0)));
  for (int col = 0; col < 5; ++col) {
    EXPECT_THAT(board.At(0, col), Eq(BLUE)) << col;
  }
  EXPECT_THAT(board.At(0, 5), Eq(INVALID));
  EXPECT_THAT(board.At(1, 0), Eq(INVALID));
}

// Every possible move covers exactly its tile's cells, and placing it fills
// exactly those cells.
TEST(BoardTest, PossibleMoveCellsMatchPlacement) {
  for (Color color : {BLUE, YELLOW, RED, GREEN}) {
    for (const Move& move : Board().AllPossibleMoves(color)) {
      const std::vector<Coord> cells = move.Cells();
      ASSERT_THAT(cells, SizeIs(kTiles[move.tile].Size()))
          << move.DebugString();
      Board board;
      ASSERT_TRUE(board.MakeMove(move)) << move.DebugString();
      int num_filled = 0;
      for (int row = 0; row < Board::kNumRows; ++row) {
        for (int col = 0; col < Board::kNumCols; ++col) {
          if (board.At(row, col) == INVALID) continue;
          ++num_filled;
          EXPECT_THAT(board.At(row, col), Eq(color));
          EXPECT_TRUE(std::find(cells.begin(), cells.end(),
                                Coord(row, col)) != cells.end())
              << move.DebugString() << " filled " << Coord(row, col);
        }
      }
      EXPECT_THAT(num_filled, Eq(cells.size())) << move.DebugString();
    }
  }
}

TEST(BoardTest, PossibleMoves_StartInRedCorner) {
  // RED starts in the bottom-right corner, so its first piece must have a
  // block in the bottom-right of its bounding box. The L tetromino has 8
  // orientations, and 6 of them have that block filled.
  Board board;
  std::vector<Move> moves = board.PossibleMoves(kTiles[7], RED);
  EXPECT_THAT(moves, SizeIs(6));
  for (const Move& move : moves) {
    EXPECT_TRUE(board.IsPossible(move)) << move.DebugString();
  }
  // For orientation (rotation 3, flip true), the transform origin is the
  // bottom-right block, so this placement has the largest coordinate on the
  // board.
  const Placement corner_placement{
      Coord(Board::kNumRows - 1, Board::kNumCols - 1), 3, true};
  EXPECT_TRUE(std::any_of(moves.begin(), moves.end(), [&](const Move& m) {
    return m.placement == corner_placement;
  }));
}

TEST(BoardTest, PossibleMoves_Start_1x1) {
  Board board;
  std::vector<Move> moves = board.PossibleMoves(kTiles[0], BLUE);
  ASSERT_THAT(moves, SizeIs(1));
  EXPECT_THAT(moves[0].placement.coord, Eq(Coord(0, 0)));
  EXPECT_TRUE(board.IsPossible(moves[0]));

  moves = board.PossibleMoves(kTiles[0], YELLOW);
  ASSERT_THAT(moves, SizeIs(1));
  EXPECT_THAT(moves[0].placement.coord, Eq(Coord(0, Board::kNumCols - 1)));
  EXPECT_TRUE(board.IsPossible(moves[0]));
  
  moves = board.PossibleMoves(kTiles[0], RED);
  ASSERT_THAT(moves, SizeIs(1));
  EXPECT_THAT(moves[0].placement.coord,
              Eq(Coord(Board::kNumRows - 1, Board::kNumCols - 1)));
  EXPECT_TRUE(board.IsPossible(moves[0]));

  moves = board.PossibleMoves(kTiles[0], GREEN);
  ASSERT_THAT(moves, SizeIs(1));
  EXPECT_THAT(moves[0].placement.coord, Eq(Coord(Board::kNumRows - 1, 0)));
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
  EXPECT_THAT(moves[0].placement.coord, Eq(Coord(0, Board::kNumCols - 2)));
  EXPECT_THAT(moves[0].placement.rotation, Eq(0));
  EXPECT_THAT(moves[0].placement.flip, Eq(false));
  EXPECT_TRUE(board.IsPossible(moves[0]));
  EXPECT_THAT(moves[1].placement.coord, Eq(Coord(1, Board::kNumCols - 1)));
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
  EXPECT_THAT(moves[0].placement.coord, Eq(Coord(2, Board::kNumCols - 3)));
  EXPECT_THAT(moves[0].placement.rotation, Eq(0));
  EXPECT_THAT(moves[0].placement.flip, Eq(true));
  EXPECT_TRUE(board.IsPossible(moves[0]));
  EXPECT_THAT(moves[1].placement.coord, Eq(Coord(2, Board::kNumCols - 3)));
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

  // Slightly more complicated, try the 2x2.
  moves = board.PossibleMoves(kTiles[1], BLUE);
  ASSERT_THAT(moves, SizeIs(6));
  EXPECT_THAT(moves[0].placement.coord, Eq(Coord(0, 2)));
  EXPECT_THAT(moves[0].placement.rotation, Eq(0));
  EXPECT_THAT(moves[0].placement.flip, Eq(false));
  EXPECT_THAT(moves[1].placement.coord, Eq(Coord(1, 3)));
  EXPECT_THAT(moves[1].placement.rotation, Eq(0));
  EXPECT_THAT(moves[1].placement.flip, Eq(false));
  EXPECT_THAT(moves[2].placement.coord, Eq(Coord(1, 3)));
  EXPECT_THAT(moves[2].placement.rotation, Eq(1));
  EXPECT_THAT(moves[2].placement.flip, Eq(false));
  EXPECT_THAT(moves[3].placement.coord, Eq(Coord(4, 0)));
  EXPECT_THAT(moves[3].placement.rotation, Eq(1));
  EXPECT_THAT(moves[3].placement.flip, Eq(false));
  EXPECT_THAT(moves[4].placement.coord, Eq(Coord(3, 3)));
  EXPECT_THAT(moves[4].placement.rotation, Eq(0));
  EXPECT_THAT(moves[4].placement.flip, Eq(false));
  EXPECT_THAT(moves[5].placement.coord, Eq(Coord(4, 3)));
  EXPECT_THAT(moves[5].placement.rotation, Eq(1));
  EXPECT_THAT(moves[5].placement.flip, Eq(false));

  // More complicated, play the L (4 total tiles).
  moves = board.PossibleMoves(kTiles[7], BLUE);
  EXPECT_THAT(moves, SizeIs(18));
  EXPECT_THAT(moves[0].placement.coord, Eq(Coord(0, 2)));
  EXPECT_THAT(moves[0].placement.rotation, Eq(0));
  EXPECT_THAT(moves[0].placement.flip, Eq(false));
  EXPECT_THAT(moves[1].placement.coord, Eq(Coord(1, 3)));
  EXPECT_THAT(moves[1].placement.rotation, Eq(0));
  EXPECT_THAT(moves[1].placement.flip, Eq(false));
  EXPECT_THAT(moves[2].placement.coord, Eq(Coord(1, 3)));
  EXPECT_THAT(moves[2].placement.rotation, Eq(0));
  EXPECT_THAT(moves[2].placement.flip, Eq(true));
}

std::string PrintMoves(const std::vector<Move>& moves) {
  std::string out;
  for (size_t i = 0; i < moves.size(); ++i) {
    absl::StrAppend(&out, i, ":", moves[i].DebugString(), "\n");
  }
  return out;
}

bool MoveLess(const Move& a, const Move& b) {
  return std::tuple(a.tile, a.placement.coord.row(), a.placement.coord.col(),
                    a.placement.rotation, a.placement.flip) <
         std::tuple(b.tile, b.placement.coord.row(), b.placement.coord.col(),
                    b.placement.rotation, b.placement.flip);
}

// Expects `board` and `ver_board` to agree on the possible moves for `color`
// with every tile in `tiles_left`. Returns all of those moves.
std::vector<Move> ExpectPossibleMovesMatch(const Board& board,
                                           const VerificationBoard& ver_board,
                                           Color color,
                                           const std::vector<bool>& tiles_left) {
  std::vector<Move> all_moves;
  for (int tile = 0; tile < kNumTiles; ++tile) {
    if (!tiles_left[tile]) continue;
    std::vector<Move> moves = board.PossibleMoves(kTiles[tile], color);
    std::vector<Move> ver_moves = ver_board.PossibleMoves(kTiles[tile], color);
    std::sort(moves.begin(), moves.end(), MoveLess);
    std::sort(ver_moves.begin(), ver_moves.end(), MoveLess);
    EXPECT_TRUE(moves == ver_moves)
        << ColorToString(color) << " tile " << tile << "\nBoard:\n"
        << PrintMoves(moves) << "VerificationBoard:\n"
        << PrintMoves(ver_moves);
    all_moves.insert(all_moves.end(), moves.begin(), moves.end());
  }
  return all_moves;
}

// Expects `board` and `ver_board` to agree on IsPossible for every orientation
// of every tile, with the origin anywhere on the board or one cell outside it.
void ExpectIsPossibleMatches(const Board& board,
                             const VerificationBoard& ver_board, Color color) {
  int num_mismatches = 0;
  std::string first_mismatch;
  for (int tile = 0; tile < kNumTiles; ++tile) {
    for (const TileOrientation& o : kTiles[tile].orientations()) {
      for (int row = -1; row <= Board::kNumRows; ++row) {
        for (int col = -1; col <= Board::kNumCols; ++col) {
          const Move move =
              MakeTestMove(color, tile, row, col, o.rotation(), o.flip());
          const bool expected = ver_board.IsPossible(move);
          if (board.IsPossible(move) != expected) {
            if (num_mismatches++ == 0) {
              first_mismatch =
                  absl::StrCat(move.DebugString(), ", expected ", expected);
            }
          }
        }
      }
    }
  }
  EXPECT_THAT(num_mismatches, Eq(0)) << "first mismatch: " << first_mismatch;
}

// Plays a random game to completion with seed `seed`, checking `Board` against
// `VerificationBoard` before every move. Colors pass when they have no moves.
// If `check_is_possible`, also checks IsPossible exhaustively, which is slow.
// Returns the number of tiles played.
int PlayVerifiedGame(int seed, bool check_is_possible) {
  Board board;
  VerificationBoard ver_board;
  std::mt19937 eng(seed);

  std::map<Color, std::vector<bool>> tiles_left;
  std::set<Color> passed;
  for (Color color : {BLUE, YELLOW, RED, GREEN}) {
    tiles_left[color] = std::vector<bool>(kNumTiles, true);
  }

  int num_moves = 0;
  for (Color color = BLUE; passed.size() < 4; color = NextColor(color)) {
    if (passed.count(color)) continue;
    SCOPED_TRACE(testing::Message() << "move " << num_moves << ", "
                                    << ColorToString(color) << "'s turn");
    if (check_is_possible) ExpectIsPossibleMatches(board, ver_board, color);
    std::vector<Move> moves =
        ExpectPossibleMovesMatch(board, ver_board, color, tiles_left[color]);
    if (testing::Test::HasFailure()) return num_moves;

    if (moves.empty()) {
      passed.insert(color);
      continue;
    }
    std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);
    const Move& move = moves[dist(eng)];
    EXPECT_TRUE(board.MakeMove(move)) << move.DebugString();
    EXPECT_TRUE(ver_board.MakeMove(move)) << move.DebugString();
    tiles_left[color][move.tile] = false;
    ++num_moves;
  }
  return num_moves;
}

TEST(BoardTest, PossibleMovesMatchVerificationBoard) {
  for (int seed = 0; seed < 20; ++seed) {
    SCOPED_TRACE(testing::Message() << "seed " << seed);
    const int num_moves = PlayVerifiedGame(seed, /*check_is_possible=*/false);
    ASSERT_FALSE(HasFailure());
    // Sanity check that the games actually went somewhere.
    EXPECT_THAT(num_moves, Gt(40));
  }
}

TEST(BoardTest, IsPossibleMatchesVerificationBoard) {
  for (int seed = 100; seed < 102; ++seed) {
    SCOPED_TRACE(testing::Message() << "seed " << seed);
    PlayVerifiedGame(seed, /*check_is_possible=*/true);
    ASSERT_FALSE(HasFailure());
  }
}

// Returns PossibleMoves for each tile `color` hasn't placed, in tile order.
std::vector<Move> AllPossibleMovesFromScratch(
    const Board& board, Color color, const std::vector<bool>& tiles_left) {
  std::vector<Move> moves;
  for (int tile = 0; tile < kNumTiles; ++tile) {
    if (!tiles_left[tile]) continue;
    std::vector<Move> tile_moves = board.PossibleMoves(kTiles[tile], color);
    moves.insert(moves.end(), tile_moves.begin(), tile_moves.end());
  }
  return moves;
}

// The incrementally tracked moves must match computing them from scratch, in
// the same order, for every color before every move.
TEST(BoardTest, AllPossibleMovesMatchPossibleMoves) {
  for (int seed = 0; seed < 20; ++seed) {
    SCOPED_TRACE(testing::Message() << "seed " << seed);
    Board board;
    std::mt19937 eng(seed);
    std::map<Color, std::vector<bool>> tiles_left;
    for (Color color : {BLUE, YELLOW, RED, GREEN}) {
      tiles_left[color] = std::vector<bool>(kNumTiles, true);
    }

    int num_moves = 0;
    int num_passes_in_a_row = 0;
    for (Color color = BLUE; num_passes_in_a_row < 4;
         color = NextColor(color)) {
      SCOPED_TRACE(testing::Message() << "move " << num_moves << ", "
                                      << ColorToString(color) << "'s turn");
      for (Color other : {BLUE, YELLOW, RED, GREEN}) {
        const std::vector<Move> moves = board.AllPossibleMoves(other);
        ASSERT_THAT(moves, ContainerEq(AllPossibleMovesFromScratch(
                               board, other, tiles_left[other])))
            << ColorToString(other);
        ASSERT_THAT(board.NumPossibleMoves(other), Eq(moves.size()));
        for (int i = 0; i < static_cast<int>(moves.size()); ++i) {
          ASSERT_THAT(board.PossibleMove(other, i), Eq(moves[i])) << i;
        }
      }

      const int num_possible = board.NumPossibleMoves(color);
      if (num_possible == 0) {
        ++num_passes_in_a_row;
        continue;
      }
      num_passes_in_a_row = 0;
      std::uniform_int_distribution<int> dist(0, num_possible - 1);
      const Move move = board.PossibleMove(color, dist(eng));
      ASSERT_TRUE(board.MakeMove(move)) << move.DebugString();
      tiles_left[color][move.tile] = false;
      ++num_moves;
    }
    EXPECT_THAT(num_moves, Gt(40));
  }
}

}  // namespace
}  // namespace blokus
