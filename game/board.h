#ifndef BLOKUS_GAME_BOARD_H_
#define BLOKUS_GAME_BOARD_H_

#include <array>
#include <cstdint>
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

  // Returns all possible moves for `color` with the tiles it hasn't placed on
  // this board yet. The result is the same, in the same order, as calling
  // PossibleMoves for each of those tiles in increasing tile order, but it is
  // maintained incrementally by MakeMove, so it's much cheaper.
  std::vector<Move> AllPossibleMoves(Color color) const;

  // Returns AllPossibleMoves(color).size(), without building the moves.
  int NumPossibleMoves(Color color) const {
    return tracked_moves_[color].size();
  }

  // Returns AllPossibleMoves(color)[index], without building all the moves.
  Move PossibleMove(Color color, int index) const;

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

  // Adds the moves for `color` that become possible through the slots from
  // index `first_new_slot` onwards. `old_slot_rows` is the slot bitmap from
  // before those slots were added.
  void AddTrackedMoves(Color color, int first_new_slot,
                       const uint32_t old_slot_rows[]);

  // Removes the tracked moves for `color` that are no longer possible, after
  // cells in rows [min_row, max_row] and columns [min_col, max_col] became
  // unavailable to it. Also removes all moves with `placed_tile`, unless it is
  // -1.
  void RemoveTrackedMoves(Color color, int min_row, int max_row, int min_col,
                          int max_col, int placed_tile);
  
  Color pieces_[kNumRows][kNumCols];

  // Bitwise representation of the rows in the board, one for each color.
  // Column 0 corresponds to the least significant bit in each uint32_t.
  // A "1" means that the (row, col) is available for a piece, while a "0" means
  // that it is occupied. Indexed by color, then row.
  std::array<std::array<uint32_t, kNumRows>, 5> available_ = {};

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
  // Indexed by color.
  std::array<std::vector<SlotInfo>, 5> slots_;

  // Bitwise representation of where each color has a slot, in the same
  // layout as available_. Used to dedup multiple slots on the same position.
  uint32_t slot_rows_[5][kNumRows];

  // A possible move, tracked incrementally. `row` and `col` are the board
  // position of the upper-left corner of the orientation's bounding box, and
  // `last_row` and `last_col` of its lower-right corner.
  struct TrackedMove {
    // The cells of the orientation, 5 bits per row of its bounding box, with
    // the same layout as TileOrientation::rows().
    uint32_t cells;
    uint8_t tile;
    uint8_t orientation;  // Index into Tile::orientations().
    int8_t row;
    int8_t col;
    int8_t last_row;
    int8_t last_col;
  };

  // Converts a tracked move for `color` to a Move.
  static Move ToMove(Color color, const TrackedMove& tracked);
  // Returns the tracked move for placing `tile` at `candidate` on `slot`.
  static TrackedMove MakeTrackedMove(int tile,
                                     const Tile::CornerPlacement& candidate,
                                     const Coord& slot);

  // For each color, all possible moves with tiles it hasn't placed. Within a
  // tile, the moves are in the order PossibleMoves would return them.
  std::vector<TrackedMove> tracked_moves_[5];
  // For each color, a bit per tile it has placed.
  uint32_t placed_tiles_[5] = {0, 0, 0, 0, 0};
};

inline bool operator==(const Move& lhs, const Move& rhs) {
  if (lhs.color != rhs.color) return false;
  if (lhs.tile != rhs.tile) return false;
  if (lhs.tile == -1) return true;
  return lhs.placement == rhs.placement;
}

}  // namespace blokus

#endif
