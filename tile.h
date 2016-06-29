#ifndef BLOKUS_TILE_H
#define BLOKUS_TILE_H

namespace blokus {

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

typedef bool Tile[5][5];

// TODO(piotrf): there is a lot of opportunity here for caching coords of
// rotations and flips. Make Tile a real class and add that.
// TODO(piotrf): would be useful to have easier indexing into tiles of
// different sizes.
extern Tile kTiles[];
  
}  // namespace blokus

#endif
