#include "board.h"

#include <cstring>
#include <glog/logging.h>

#define ANSI_COLOR_RESET   "\x1b[0m"

namespace blokus {

namespace {

// Returns all translated, rotated, and flipped coordinates for the placed
// tile.
// TODO(piotrf): consider exposing this as a public api.
std::vector<Coord> PlacedTile(const Tile& tile, const Move& move) {
  std::vector<Coord> coors;
  for (int i = 0; i < 5; ++i) {
    for (int j = 0; j < 5; ++j) {
      if (tile[i][j]) {
        Coord c;
        switch (move.rotation) {
          case 0: c[0] = i; c[1] = j; break;
          case 1: c[0] = -j; c[1] = i; break;
          case 2: c[0] = -i; c[1] = -j; break;
          case 3: c[0] = j; c[1] = -i; break;
          default:
            LOG(FATAL) << "Unknown rotation: " << move.rotation;
        }
        if (move.flip) {
          c[0] = -c[0];
        }
        coors.push_back(std::move(c));
      }
    }
  }
  for (auto& coor : coors) {
    coor[0] += move.coord[0];
    coor[1] += move.coord[1];
  }
  return coors;
}

}  // namespace

std::string Move::DebugString() const {
  char output[12];
  snprintf(output, 12, "(%2d,%2d) %d %d", coord[0], coord[1], rotation, flip);
  return output;
}

std::string ColorToString(Color c) {
  switch(c) {
    case BLUE: return "Blue";
    case YELLOW: return "Yellow";
    case RED: return "Red";
    case GREEN: return "Green";
    default:
      LOG(FATAL) << "Unknown color: " << c;
  }
}

std::string AnsiColor(Color c) {
  switch(c) {
    case BLUE: return "\x1b[34m";
    case YELLOW: return "\x1b[33m";
    case RED: return "\x1b[31m";
    case GREEN: return "\x1b[32m";
    default:
      LOG(FATAL) << "Unknown color: " << c;
  }
}

Board::Board() {
  // Initially, there are no pieces on the board.
  memset(pieces_, 0, kWidth * kHeight * sizeof(uint8_t));

  // Initially, you can only move in a corner. Place in turn order,
  // going clockwise from top-left (0,0).
  memset(frontier_, 0, kWidth * kHeight * sizeof(uint8_t));
  frontier_[0][0] = BLUE;
  frontier_[kWidth - 1][0] = YELLOW;
  frontier_[kWidth - 1][kHeight - 1] = RED;
  frontier_[0][kHeight - 1] = GREEN;

  // Initially, everyone is allowed to move everywhere.
  memset(allowed_, BLUE | YELLOW | RED | GREEN,
         kWidth * kHeight * sizeof(uint8_t));
}

Board::~Board() {
}

// Returns true if it is possible to place the given tile, of the given color,
// in the given move, respecting geometry of the board and rules of the game.
bool Board::IsPossible(const Tile& tile, Color color, const Move& move) const {
  bool contains_frontier = false;
  for (const Coord& coord : PlacedTile(tile, move)) {
    int x = coord[0];
    int y = coord[1];

    // Verify that the piece is on the board.
    if (x < 0 || x > kWidth - 1) return false;
    if (y < 0 || y > kHeight - 1) return false;

    // Verify that we are allowed to move here.
    if ((allowed_[x][y] & color) == 0) return false;

    // Keep track of whether or not we touch the frontier. We must touch in at
    // least on place.
    contains_frontier |= frontier_[coord[0]][coord[1]] & color;
  }
  return contains_frontier;
}

// Returns a list of all possible moves for the given tile and color.
// TODO(piotrf): currently, this can list duplicates of essentially the same
// move, since the resulting board state may be the same for different
// coordinates and a different rotation.
std::vector<Move> Board::PossibleMoves(const Tile& tile, Color color) const {
  std::vector<Move> moves;

  // Naive, brute-force algorithm: try all possible places on the board, with
  // all rotations and flips.
  // TODO(piotrf): implement a more efficient search that ignores things that
  // obviously don't work.
  for (int x = 0; x < kWidth; ++x) {
    for (int y = 0; y < kHeight; ++y) {
      for (int rotation = 0; rotation < 4; rotation++) {
        for (bool flip : {false, true}) {
          Move move;
          move.coord = Coord(x, y);
          move.rotation = rotation;
          move.flip = flip;
          if (IsPossible(tile, color, move)) {
            moves.push_back(std::move(move));
          }
        }
      }
    }
  }

  return moves;
}

bool Board::Place(const Tile& tile, Color color, const Move& move) {
  if (!IsPossible(tile, color, move)) {
    return false;
  }
  std::vector<Coord> coords = PlacedTile(tile, move);
  for (const Coord& coord : coords) {
    pieces_[coord[0]][coord[1]] = color;
  }
  // Update frontier and allowed based on current state of pieces_.
  for (int i = 0; i < kWidth; ++i) {
    for (int j = 0; j < kHeight; ++j) {
      // If there is a piece here, it's not on the frontier or allowed.
      if (pieces_[i][j]) {
        frontier_[i][j] = 0;
        allowed_[i][j] = 0;
        continue;
      }
      // If there is a piece horizontally or vertically separated, then
      // that color can't move here.
      uint8_t allowed = BLUE | YELLOW | RED | GREEN;
      if (i > 0 && pieces_[i - 1][j]) allowed &= ~pieces_[i - 1][j];
      if (i < kWidth - 1 && pieces_[i + 1][j]) allowed &= ~pieces_[i + 1][j];
      if (j > 0 && pieces_[i][j - 1]) allowed &= ~pieces_[i][j - 1];
      if (j < kHeight - 1 && pieces_[i][j + 1]) allowed &= ~pieces_[i][j + 1];
      allowed_[i][j] = allowed;
      // If there is a piece corner separated from us, and this is an allowed
      // spot, then this is on our frontier.
      uint8_t frontier = 0;
      if (i > 0 && j > 0 && pieces_[i - 1][j - 1])
        frontier |= pieces_[i - 1][j - 1];
      if (i > 0 && j < kHeight - 1 && pieces_[i - 1][j + 1])
        frontier |= pieces_[i - 1][j + 1];
      if (i < kWidth - 1 && j > 0 && pieces_[i + 1][j - 1])
        frontier |= pieces_[i + 1][j - 1];
      if (i < kWidth - 1 && j < kHeight - 1 && pieces_[i + 1][j + 1])
        frontier |= pieces_[i + 1][j + 1];
      frontier_[i][j] = frontier & allowed;
    }
  }
  // Always enforce initial frontier.
  // TODO(piotrf): clean up code duplication.
  frontier_[0][0] = BLUE;
  frontier_[kWidth - 1][0] = YELLOW;
  frontier_[kWidth - 1][kHeight - 1] = RED;
  frontier_[0][kHeight - 1] = GREEN;
  return true;
}

void Board::PrintAllowable(const uint8_t board[kWidth][kHeight]) const {
  printf("  ");
  for (int i = 0; i < kWidth; ++i) {
    printf("  %2d ", i);
  }
  printf("\n");
  for (int j = 0; j < kHeight; ++j) {
    printf("%2d ", j);
    for (int i = 0; i < kWidth; ++i) {
      std::string out;
      for (Color color : {BLUE, YELLOW, RED, GREEN}) {
        if (board[i][j] & color)
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

void Board::Print(bool debug) const {
  // Print out the pieces on the board.
  printf("  ");
  for (int i = 0; i < kWidth; ++i) {
    printf(" %2d", i);
  }
  printf("\n");
  for (int j = 0; j < kHeight; ++j) {
    printf("%2d ", j);
    for (int i = 0; i < kWidth; ++i) {
      if (pieces_[i][j]) {
        std::string out =
            AnsiColor(static_cast<Color>(pieces_[i][j])) + "\u25a3";
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

}  // namespace blokus
