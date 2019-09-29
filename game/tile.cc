#include "game/tile.h"

#include <glog/logging.h>

namespace blokus {

// TODO(piotrf): don't construct non-POD globals!
Tile kTiles[] = {
  // 1 block tiles.
  Tile(0, {
    {1, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  // 2 block tiles.
  Tile(1, {
    {1, 1, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  // 3 block tiles.
  Tile(2, {
    {1, 1, 0, 0, 0},
    {0, 1, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  Tile(3, {
    {1, 1, 1, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  // 4 block tiles.
  Tile(4, {
    {1, 1, 0, 0, 0},
    {1, 1, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  Tile(5, {
    {0, 1, 0, 0, 0},
    {1, 1, 1, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  Tile(6, {
    {1, 1, 1, 1, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  Tile(7, {
    {1, 1, 1, 0, 0},
    {0, 0, 1, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  Tile(8, {
    {1, 1, 0, 0, 0},
    {0, 1, 1, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  // 5 block tiles.
  Tile(9, {
    {1, 0, 0, 0, 0},
    {1, 1, 1, 1, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  Tile(10, {
    {1, 0, 0, 0, 0},
    {1, 1, 1, 0, 0},
    {1, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  Tile(11, {
    {1, 0, 0, 0, 0},
    {1, 0, 0, 0, 0},
    {1, 1, 1, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  Tile(12, {
    {1, 1, 0, 0, 0},
    {0, 1, 1, 1, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  Tile(13, {
    {1, 0, 0, 0, 0},
    {1, 1, 1, 0, 0},
    {0, 0, 1, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  Tile(14, {
    {1, 1, 1, 1, 1},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  Tile(15, {
    {1, 1, 1, 0, 0},
    {0, 1, 1, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  Tile(16, {
    {1, 0, 0, 0, 0},
    {1, 1, 0, 0, 0},
    {0, 1, 1, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  Tile(17, {
    {1, 1, 0, 0, 0},
    {1, 0, 0, 0, 0},
    {1, 1, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  Tile(18, {
    {1, 1, 0, 0, 0},
    {0, 1, 1, 0, 0},
    {0, 1, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  Tile(19, {
    {0, 1, 0, 0, 0},
    {1, 1, 1, 0, 0},
    {0, 1, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
  Tile(20, {
    {1, 1, 1, 1, 0},
    {0, 1, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
  }),
};

Tile::Tile(int index, std::vector<std::vector<bool>> blocks)
    : index_(index), blocks_(std::move(blocks)) {
  CHECK_EQ(5, blocks_.size());
  for (int rotation = 0; rotation < 4; ++rotation) {
    for (bool flip: {true, false}) {
      auto& coors = transforms_[rotation][flip];
      for (int i = 0; i < 5; ++i) {
        CHECK_EQ(5, blocks_[i].size());
        for (int j = 0; j < 5; ++j) {
          if (blocks_[i][j]) {
            Coord c;
            switch (rotation) {
              case 0: c[0] = i; c[1] = j; break;
              case 1: c[0] = -j; c[1] = i; break;
              case 2: c[0] = -i; c[1] = -j; break;
              case 3: c[0] = j; c[1] = -i; break;
            }
            if (flip) c[0] = -c[0];
            coors.push_back(std::move(c));
          }
        }
      }
    }
  }
} 

const std::vector<Coord>& Tile::Transform(int rotation, bool flip) const {
  CHECK_GE(rotation, 0);
  CHECK_LT(rotation, 4);
  return transforms_[rotation][flip];
}

int Tile::Size() const {
  // All transforms should have the same number of blocks.
  return transforms_[0][0].size();
}

}  // namespace blokus
