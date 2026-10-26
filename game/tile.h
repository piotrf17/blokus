#ifndef BLOKUS_GAME_TILE_H
#define BLOKUS_GAME_TILE_H

#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include "absl/types/span.h"

#include "game/defs.h"

namespace blokus {

// A "slot" represents a place where a "corner" can fit.
// Each tile has a number of slots, at the locations where another piece could
// connect. A slot also has a "type", which represents the direction the slot is
// facing. As pieces are added to the board, their slots are merged into a list
// of all available slots for a player, which heavily restricts the search space
// for finding available moves.
struct Slot {
  enum Type {   
    INVALID = 0,
    NORTH   = 1,
    WEST    = 2,
    EAST    = 3,
    SOUTH   = 4,
    SE      = 5,
    NE      = 6,
    NW      = 7,
    SW      = 8,
  };
    
  Coord c;
  Type type;
};

// A "corner" indicates a square on a tile that can be placed next (corner-wise)
// to another piece of the same corner. The type of the corner needs to
// correspond with the type of a slot. For example, an EAST slot can only take
// a WEST corner. However, a NE slot can accept a SOUTH, WEST and SW corner.
struct Corner {
  enum Type {
    INVALID = 0,
    NORTH   = 1,
    WEST    = 2,
    EAST    = 3,
    SOUTH   = 4,
    SE      = 5,
    NE      = 6,
    NW      = 7,
    SW      = 8,
    ALL     = 9,
  };

  Coord c;
  Type type;
};

inline bool CornerFitsSlot(const Corner& corner, const Slot& slot) {
  static bool fit_map[10][9] = {
  // I  N  W  E  S  SE NE NW SW
    {0, 0, 0, 0, 0, 0, 0, 0, 0},  // INVALID
    {0, 0, 0, 0, 1, 1, 0, 0, 1},  // NORTH
    {0, 0, 0, 1, 0, 1, 1, 0, 0},  // WEST
    {0, 0, 1, 0, 0, 0, 0, 1, 1},  // EAST
    {0, 1, 0, 0, 0, 0, 1, 1, 0},  // SOUTH
    {0, 0, 0, 0, 0, 0, 0, 1, 0},  // SE
    {0, 0, 0, 0, 0, 0, 0, 0, 1},  // NE
    {0, 0, 0, 0, 0, 1, 0, 0, 0},  // NW
    {0, 0, 0, 0, 0, 0, 1, 0, 0},  // SW
    {0, 1, 1, 1, 1, 1, 1, 1, 1},  // ALL
  };
  return fit_map[corner.type][slot.type];
}

// An 8x8 region of the board around a slot, which contains every square a
// tile can cover when one of its corners is placed on a slot of that type.
// `row` and `col` give the window's upper-left square relative to the slot.
struct SlotWindow {
  int row;
  int col;
};

inline SlotWindow GetSlotWindow(Slot::Type type) {
  static constexpr SlotWindow kWindows[9] = {
      {0, 0},    // INVALID
      {-4, -3},  // NORTH
      {-3, -4},  // WEST
      {-3, 0},   // EAST
      {0, -3},   // SOUTH
      {-3, -3},  // SE
      {-4, -3},  // NE
      {-4, -4},  // NW
      {-3, -4},  // SW
  };
  return kWindows[type];
}


// A predetermined orientation of a tile.
// Each orientation corresponds to a physically different placement of a tile.
// For example, all rotations and flips of a 1x1 tile lead to the same physical
// placement, so the 1x1 tile will have a only a single TileOrientation.
class TileOrientation {
 public:
  TileOrientation(int rotation, bool flip, std::vector<Coord> coords,
                  Coord offset);
  
  absl::Span<const Coord> coords() const { return coords_; }

  absl::Span<const Slot> slots() const { return slots_; }
  absl::Span<const Corner> corners() const { return corners_; }

  int rotation() const { return rotation_; }
  bool flip() const { return flip_; }
  Coord offset() const { return offset_; }

  // Size of the tile in this orientation, in number of rows and columns.
  int num_rows() const { return num_rows_; }
  int num_cols() const { return num_cols_; }

  // Bitmap representation of the rows of this tile.
  // The least significant digit corresponds to the 0 column.
  //
  // For example, a tile like:
  //
  //   oo
  //   o
  //   oo
  //
  // Will have rows:
  //
  //   0b11
  //   0b01
  //   0b11
  absl::Span<const uint32_t> rows() const { return rows_; }

  // The rows() packed into one integer, 5 bits per row: row r is in bits
  // 5 * r to 5 * r + 4.
  uint32_t packed_rows() const { return packed_rows_; }

  // Like the above, but padded by one block in all adjacent directions.
  // For the example above, this will be:
  //
  //   0b0110
  //   0b1111
  //   0b0111
  //   0b1111
  //   0b0110
  absl::Span<const uint32_t> expanded_rows() const {
    return expanded_rows_;
  }

 private:
  void ComputeSlotsAndCorners();
  void ComputeSlices();
  
  int rotation_;
  bool flip_;
  std::vector<Coord> coords_;
  Coord offset_;
  std::vector<Slot> slots_;
  std::vector<Corner> corners_;

  int num_rows_ = 0;
  int num_cols_ = 0;
  std::vector<uint32_t> rows_;
  uint32_t packed_rows_ = 0;
  std::vector<uint32_t> expanded_rows_;
};

// A Blokus piece. Each tile is defined on a 5x5 grid of blocks, using (row,
// col) coordinates with (0, 0) at the upper-left. Every tile has at least one
// block in row 0 and in column 0, but block (0, 0) itself may be empty.
//
// Transform() rotates the blocks counter-clockwise (as drawn, with rows
// increasing downwards) around (0, 0), in 90 degree steps. If flip is set, the
// rotated blocks are then mirrored top-to-bottom (row -> -row). Transformed
// coordinates may be negative. For example (where x marks (0, 0)):
//
// original       rotation=1        rotation=2       rotation=3
// --------       ----------        ----------       ----------
//   x                oo               o                 ox
//   ooo              o                ooo               o
//     o             xo                  x              oo
//
// flipped        rotation=1        rotation=2       rotation=3
// --------       ----------        ----------       ----------
//     o             xo                  x              oo
//   ooo              o                ooo               o
//   x                oo               o                 ox
//
// Several transforms can give the same shape. orientations() holds one
// TileOrientation per physically distinct shape, and moves on the board must
// use the (rotation, flip) of one of those. Canonicalize() converts any
// placement to that form.
class Tile {
 public:
  explicit Tile(int index, std::vector<std::vector<bool>> blocks);

  // Return the filled in coordinates for this tile, given the specified
  // transformation.
  const std::vector<Coord>& Transform(int rotation, bool flip) const;

  // How many blocks in the tile?
  int Size() const;

  int index() const { return index_; }

  const std::vector<TileOrientation>& orientations() const {
    return orientations_;
  }

  Placement Canonicalize(Placement placement) const;

  // One way to place this tile with one of its corners on a slot.
  struct CornerPlacement {
    // The placement, with the coordinate relative to the slot.
    Placement placement;
    // Index of the placement's orientation in orientations().
    int orientation;
    // The squares the tile covers, as a bitmap over the slot's SlotWindow.
    // Square (row, col) of the window is bit row * 8 + col.
    uint64_t mask;
  };

  // Returns a placement for every corner, over all orientations, that fits a
  // slot of the given type (see CornerFitsSlot), ordered by orientation and
  // then by corner.
  absl::Span<const CornerPlacement> CornerPlacements(Slot::Type type) const {
    return corner_placements_[type];
  }

 private:
  void ComputeTransformations();
  void ComputeOrientations();
  void ComputeCornerPlacements();

  int index_;
  std::vector<std::vector<bool>> blocks_;
  std::vector<Coord> transforms_[4][2];
  std::vector<TileOrientation> orientations_;
  // Indexed by Slot::Type.
  std::vector<CornerPlacement> corner_placements_[9];
};

// TODO(piotrf): would be useful to have easier indexing into tiles of
// different sizes.
extern Tile kTiles[];

inline constexpr int kNumTiles = 21;
  
}  // namespace blokus

#endif
