#ifndef BLOKUS_GAME_BOARD_H_
#define BLOKUS_GAME_BOARD_H_

#include <map>
#include <string>
#include <vector>

#include "game/defs.h"
#include "game/tile.h"

namespace blokus {

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
  static const int kNumRows = 20;
  static const int kNumCols = 20;
  
  Board();
  ~Board();

  // Returns true if the given move respects the geometry of the board and rules
  // of the game. Note that Board does not track the current player or the
  // tiles they have. Use 'Game' for that.
  // TODO(piotrf): change this API to take a TileOrientation instead.
  bool IsPossible(const Move& move) const;

  // Returns a list of all possible moves for the given tile and color.
  std::vector<Move> PossibleMoves(const Tile& tile, Color color) const;

  // Place a tile on the board. Returns true if the move was valid.
  // An invalid move will not change the state of the board.
  bool MakeMove(const Move& move);

  // Print the board to stdout, with terminal colors.
  // TODO(piotrf): return a string instead.
  void Print(bool debug = false) const;
  
 private:
  bool IsPossible(const Slot& slot,
                  const TileOrientation& orientation,
                  const Corner& corner, Color color) const;
  
  Color pieces_[kNumRows][kNumCols];

  // Bitwise representation of the rows in the board, one for each color.
  // Column 0 corresponds to the least significant bit in each uint32_t.
  // A "1" means that the (row, col) is available for a piece, while a "0" means
  // that it is occupied.
  std::vector<std::vector<uint32_t>> available_;

  // Information about a "slot" on the board.
  // A slot represents a possible place that a piece corner could go, basically
  // any spot next to a corner of an existing piece.
  struct SlotInfo {
    // The actual slot itself.
    Slot slot;
    // A cache of possible tiles on this slot.
    // This is mutable so that it can be updated in the const PossibleMoves.
    // TODO(piotrf): consider refactoring the API, this is a hack.
    mutable uint32_t possible_tiles;

    explicit SlotInfo(const Slot& a_slot)
        : slot(a_slot), possible_tiles(0xffffffff) {}

    bool IsAvailable(const Tile& tile) const {
      return possible_tiles & (1 << tile.index());
    }

    void SetUnavailable(const Tile& tile) const {
      possible_tiles &= ~(1 << tile.index());
    }
  };
  std::vector<std::vector<SlotInfo>> slots_;

  // A map from a position on the board, represented by a flat integer,
  // to whether or not a slot exists at that position. Used to dedup multiple
  // slots on the same position.
  std::vector<std::vector<bool>> slot_map_;
};

inline bool operator==(const Move& lhs, const Move& rhs) {
  if (lhs.color != rhs.color) return false;
  if (lhs.tile != rhs.tile) return false;
  if (lhs.tile == -1) return true;
  return lhs.placement == rhs.placement;
}

}  // namespace blokus

#endif
