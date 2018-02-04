#ifndef BLOKUS_GAME_BOARD_H_
#define BLOKUS_GAME_BOARD_H_

#include <string>
#include <vector>

#include "game/tile.h"

namespace blokus {

// Move represents how to place a Tile on the board, although it does not
// include the tile itself.
struct Move {
  Coord coord;
  int rotation;  // 0..3
  bool flip;
  std::string DebugString() const;
};

enum Color {
  BLUE = 0x1,
  YELLOW = 0x2,
  RED = 0x4,
  GREEN = 0x8
};

std::string ColorToString(Color c);
  
class Board {
 public:
  static const int kWidth = 20;
  static const int kHeight = 20;
  
  Board();
  ~Board();

  // Returns true if it is possible to place the given tile, of the given color,
  // in the given move, respecting geometry of the board and rules of the game.
  bool IsPossible(const Tile& tile, Color color, const Move& move) const;

  // Returns a list of all possible moves for the given tile and color.
  // TODO(piotrf): currently, this can list duplicates of essentially the same
  // move, since the resulting board state may be the same for different
  // coordinates and a different rotation.
  std::vector<Move> PossibleMoves(const Tile& tile, Color color) const;

  // Place a tile on the board.
  bool Place(const Tile& tile, Color color, const Move& move);

  // Print the board to stdout, with terminal colors.
  // If debug=true, also print out representiations of frontier_ and allowed_.
  void Print(bool debug = false) const;
  
 private:
  void PrintAllowable(const uint8_t board[kWidth][kHeight]) const;
  
  uint8_t pieces_[kWidth][kHeight];
  // Frontier is the outer border of our movements. Every move must contain
  // a bit of frontier.
  uint8_t frontier_[kWidth][kHeight];
  uint8_t allowed_[kWidth][kHeight];
};

}  // namespace blokus

#endif
