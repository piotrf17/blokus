#ifndef BLOKUS_GAME_BOARD_H_
#define BLOKUS_GAME_BOARD_H_

#include <map>
#include <string>
#include <vector>

#include "game/tile.h"

namespace blokus {

enum Color {
  INVALID = 0x0,
  BLUE = 0x1,
  YELLOW = 0x2,
  RED = 0x4,
  GREEN = 0x8
};

Color NextColor(Color c);
std::string ColorToString(Color c);

// Placement represents how to place a Tile on the board, although it does not
// include the tile itself.
struct Placement {
  Coord coord;
  int rotation = 0;  // 0..3
  bool flip = false;
  std::string DebugString() const;
};

// Move fully represents a single move in the game.
struct Move {
  static Move EmptyMove(Color color);
  std::string DebugString() const;
  
  Color color = INVALID;  // Who made the move.
  int tile = -1;          // If -1, a pass. Otherwise the tile that was played.
  Placement placement;    // How the tile was played.
};

class Board {
 public:
  static const int kWidth = 20;
  static const int kHeight = 20;
  
  Board();
  ~Board();

  // Returns true if the given move respects the geometry of the board and rules
  // of the game. Note that Board does not track the current player or the
  // tiles they have. Use 'Game' for that.
  bool IsPossible(const Move& move) const;

  // Returns a list of all possible moves for the given tile and color.
  // TODO(piotrf): currently, this can list duplicates of essentially the same
  // move, since the resulting board state may be the same for different
  // coordinates and a different rotation.
  std::vector<Move> PossibleMoves(const Tile& tile, Color color) const;

  // Place a tile on the board. Returns true if the move was valid.
  // An invalid move will not change the state of the board.
  bool MakeMove(const Move& move);

  // Print the board to stdout, with terminal colors.
  // If debug=true, also print out representiations of frontier_ and allowed_.
  // TODO(piotrf): return a string instead.
  void Print(bool debug = false) const;
  
 private:
  void PrintAllowable(const uint8_t board[kWidth][kHeight]) const;

  bool IsPossible(const Slot& slot,
                  const TileOrientation& orientation,
                  const Corner& corner, Color color) const;
  
  uint8_t pieces_[kWidth][kHeight];
  // Frontier is the outer border of our movements. Every move must contain
  // a bit of frontier.
  uint8_t frontier_[kWidth][kHeight];
  uint8_t allowed_[kWidth][kHeight];

  std::map<Color, std::vector<Slot>> slots_;
};

inline bool operator==(const Placement& lhs, const Placement& rhs) {
  return lhs.coord == rhs.coord &&
      lhs.rotation == rhs.rotation &&
      lhs.flip == rhs.flip;
}

inline bool operator==(const Move& lhs, const Move& rhs) {
  if (lhs.color != rhs.color) return false;
  if (lhs.tile != rhs.tile) return false;
  if (lhs.tile == -1) return true;
  return lhs.placement == rhs.placement;
}

}  // namespace blokus

#endif
