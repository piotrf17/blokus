#include "game/board.h"

#include <random>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#define ANSI_COLOR_RESET   "\x1b[0m"

namespace blokus {
namespace {

using ::testing::Eq;
using ::testing::SizeIs;

std::vector<Coord> PlacedTile(const Tile& tile, const Placement& placement) {
  std::vector<Coord> coors = tile.Transform(placement.rotation, placement.flip);
  for (auto& coor : coors) {
    coor[0] += placement.coord[0];
    coor[1] += placement.coord[1];
  }
  return coors;
}

uint8_t ColorToByte(Color color) {
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
  void Print(bool debug = false) const;

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

TEST(BoardTest, IsPossible) {
  Board board;
  // The 1x1 tile should be possible at the start in any orientation, but
  // not anywhere else on the board.
  // TODO(piotrf): re-enable this once we settle on a better API for moves,
  // either involving TileOrientation, or de-duping the passed in orientation
  // against a known one.
  /* 
  for (int rotation = 0; rotation < 4; rotation++) {
    for (bool flip : {false, true}) {
      for (int8_t x = 0; x < Board::kNumCols; ++x) {
        for (int8_t y = 0; y < Board::kNumRows; ++y) {
          if (x == 0 && y == 0) {
            EXPECT_TRUE(board.IsPossible({BLUE, 0, {{x, y}, rotation, flip}}));
          } else {
            EXPECT_FALSE(board.IsPossible({BLUE, 0, {{x, y}, rotation, flip}}));
          }
        }
      }
    }
  }
  */
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
  for (int i = 0; i < moves.size(); ++i) {
    absl::StrAppend(&out, i, ":", moves[i].DebugString(), "\n");
  }
  return out;
}

// Plays a random game, comparing possible moves when using Board vs. possible
// moves when using VerificationBoard.
TEST(BoardTest, MatchesVerificationBoard) {
  Board board;
  VerificationBoard ver_board;

  std::mt19937 eng(0);

  // Sorting comparator for moves.
  auto compare =
      [](const Move& a, const Move& b) {
        if (a.placement.coord.row() < b.placement.coord.row()) return true;
        if (a.placement.coord.row() > b.placement.coord.row()) return false;
 
        if (a.placement.coord.col() < b.placement.coord.col()) return true;
        if (a.placement.coord.col() > b.placement.coord.col()) return false;

        if (a.placement.rotation < b.placement.rotation) return true;
        if (a.placement.rotation > b.placement.rotation) return false;

        if (!a.placement.flip) return true;
      };

  // Everyone starts with all tiles.
  std::map<Color, std::vector<bool>> player_tiles;
  player_tiles[BLUE] = std::vector<bool>(kNumTiles, true);
  player_tiles[YELLOW] = std::vector<bool>(kNumTiles, true);
  player_tiles[RED] = std::vector<bool>(kNumTiles, true);
  player_tiles[GREEN] = std::vector<bool>(kNumTiles, true);

  for (int round = 0; round < 10; ++round) {
    LOG(INFO) << "Round " << round;
    for (Color color : {BLUE, YELLOW, RED, GREEN}) {
      LOG(INFO) << ColorToString(color) << "'s turn";
      std::vector<Move> all_moves;
      for (int tile = 0; tile < kNumTiles; ++tile) {
        if (!player_tiles[color][tile]) continue;
        
        LOG(INFO) << "  Looking at tile " << tile;
        // Get possible moves using both boards, and compare.
        std::vector<Move> moves = board.PossibleMoves(kTiles[tile], color);
        std::vector<Move> ver_moves =
            ver_board.PossibleMoves(kTiles[tile], color);
        std::sort(moves.begin(), moves.end(), compare);
        std::sort(ver_moves.begin(), ver_moves.end(), compare);
        ASSERT_THAT(moves.size(), Eq(ver_moves.size()))
            << PrintMoves(moves) << "\n" << PrintMoves(ver_moves);
        for (int i = 0; i < moves.size(); ++i) {
          ASSERT_THAT(moves[i].placement.coord,
                      Eq(ver_moves[i].placement.coord));
          ASSERT_THAT(moves[i].placement.rotation,
                      Eq(ver_moves[i].placement.rotation));
          ASSERT_THAT(moves[i].placement.flip,
                      Eq(ver_moves[i].placement.flip));
        }
        all_moves.insert(all_moves.end(), moves.begin(), moves.end());
      }

      // Now, pick a move at random and make it.
      std::uniform_int_distribution<int> dist(0, all_moves.size() - 1);
      int move_idx = dist(eng);
      LOG(INFO) << "  => Making move " << all_moves[move_idx].DebugString();
      board.MakeMove(all_moves[move_idx]);
      ver_board.MakeMove(all_moves[move_idx]);
      player_tiles[color][all_moves[move_idx].tile] = false;
      board.Print(true);
    }
  }
}

}  // namespace
}  // namespace blokus
