#ifndef BLOKUS_GAME_TILE_H
#define BLOKUS_GAME_TILE_H

#include <array>
#include <cstdint>
#include <vector>

namespace blokus {

// Coord represents a a 2-d coordinate, either local to a given tile,
// or on the board. In the latter case (0,0) represents the upper-left.
// TODO(piotrf): this should probably go in a separate file (along with
// Move and Color.
struct Coord {
  Coord() {
    c[0] = 0;
    c[1] = 0;
  }
  Coord(uint8_t x, uint8_t y) {
    c[0] = x;
    c[1] = y;
  }

  uint8_t operator[](int i) const { return c[i]; }
  uint8_t& operator[](int i) { return c[i]; }
  
  uint8_t c[2];
};


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
  explicit Tile(std::vector<std::vector<bool>> blocks);

  // Return the filled in coordinates for this tile, given the specified
  // transformation.
  const std::vector<Coord>& Transform(int rotation, bool flip) const;

  // How many blocks in the tile?
  int Size() const;
  
 private:
  std::vector<std::vector<bool>> blocks_;
  std::vector<Coord> transforms_[4][2];
};

// TODO(piotrf): there is a lot of opportunity here for caching coords of
// rotations and flips. Make Tile a real class and add that.
// TODO(piotrf): would be useful to have easier indexing into tiles of
// different sizes.
extern Tile kTiles[];
  
}  // namespace blokus

#endif
