#ifndef BLOKUS_GAME_TILE_H
#define BLOKUS_GAME_TILE_H

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

namespace blokus {

// Coord represents a 2-d coordinate, either local to a given tile or on the
// board. In both cases, think of this coordinate as the (row, column) in a
// matrix. (0, 0) represents the upper-left, while (0, Board::kWidth) is the
// upper-right.
// TODO(piotrf): this should probably go in a separate file (along with
// Move and Color.
struct Coord {
  Coord() {
    c[0] = 0;
    c[1] = 0;
  }
  Coord(int8_t row, int8_t col) {
    c[0] = row;
    c[1] = col;
  }

  int8_t operator[](int i) const { return c[i]; }
  int8_t& operator[](int i) { return c[i]; }

  int8_t row() const { return c[0]; }
  int8_t col() const { return c[1]; }
  
  int8_t c[2];
};

inline bool operator==(const Coord& lhs, const Coord& rhs) {
  return lhs.c[0] == rhs.c[0] && lhs.c[1] == rhs.c[1];
}

inline std::ostream& operator<<(std::ostream& os, const Coord& c) {
  os << "(" << std::setw(2) << std::setfill(' ') << int(c.row())
     << "," << std::setw(2) << std::setfill(' ') << int(c.col()) << ")";
  return os;
}

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


// A predetermined orientation of a tile.
// Each orientation corresponds to a physically different placement of a tile.
// For example, all rotations and flips of a 1x1 tile lead to the same physical
// placement, so the 1x1 tile will have a only a single TileOrientation.
class TileOrientation {
 public:
  TileOrientation(int rotation, bool flip, std::vector<Coord> coords,
                  Coord offset);
  
  const std::vector<Coord>& coords() const { return coords_; }

  const std::vector<Slot>& slots() const { return slots_; }
  const std::vector<Corner>& corners() const { return corners_; }

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
  const std::vector<uint32_t>& rows() const { return rows_; }

  // Like the above, but padded by one block in all adjacent directions.
  // For the example above, this will be:
  //
  //   0b0110
  //   0b1111
  //   0b0111
  //   0b1111
  //   0b0110
  const std::vector<uint32_t>& expanded_rows() const { return expanded_rows_; }

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
  std::vector<uint32_t> expanded_rows_;
};

// TODO(piotrf): update this comment, parts of it are not true anymore.
// All tiles are placed on a 5x5 block. The top-left corner is (0,0), and it
// is guaranteed that a piece of the tile exists there. The coordinate system
// increases in x to the right, and in y towards the bottom.
// Rotation of a tile is done around the top-left corner, and proceeds
// clockwise. e.g. (where x represents (0,0)):
//
// original       rotation=1        rotation=2       rotation=3
// --------       ----------        ----------       ----------
//   x                ox               o                 oo
//   ooo              o                ooo               o
//     o             oo                  x              xo
//
// Flipping is always done around the y-axis. So the flipped versions of the
// above tile is:
//
// flipped        rotation=1        rotation=2       rotation=3
// --------       ----------        ----------       ----------
//     x             xo                  o              oo
//   ooo              o                ooo               o
//   o                oo               x                 ox

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
  
 private:
  void ComputeTransformations();
  void ComputeOrientations();
  
  int index_;
  std::vector<std::vector<bool>> blocks_;
  std::vector<Coord> transforms_[4][2];
  std::vector<TileOrientation> orientations_;
};

// TODO(piotrf): would be useful to have easier indexing into tiles of
// different sizes.
extern Tile kTiles[];

inline constexpr int kNumTiles = 21;
  
}  // namespace blokus

#endif
