#include "game/tile.h"

#include <unordered_set>

#include <glog/logging.h>

namespace blokus {
namespace {

// Test to see if two lists of coordinates match.
// The coordinates do not necessarily have to be in any order.
// NB: this is only for normalized coordinates on a Tile, i.e. the values are
// from 0 to 5.
bool CoordsMatch(const std::vector<Coord>& a, const std::vector<Coord>& b) {
  CHECK_EQ(a.size(), b.size());
  std::unordered_set<int> a_hashes;
  for (const Coord& coord : a) {
    const int hash = 10 * coord[0] + coord[1];
    a_hashes.insert(hash);
  }
  for (const Coord& coord : b) {
    const int hash = 10 * coord[0] + coord[1];
    if (a_hashes.count(hash) == 0) {
      return false;
    }
  }
  return true;
}


}  // namespace

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

bool Corner::Fits(const Slot& slot) const {
  switch (type) {
    case NORTH:
      return slot.type == Slot::SOUTH ||
          slot.type == Slot::SW ||
          slot.type == Slot::SE;
    case WEST:
      return slot.type == Slot::EAST ||
          slot.type == Slot::SE ||
          slot.type == Slot::NE;
    case EAST:
      return slot.type == Slot::WEST ||
          slot.type == Slot::SW ||
          slot.type == Slot::NW;
    case SOUTH:
      return slot.type == Slot::NORTH ||
          slot.type == Slot::NW ||
          slot.type == Slot::NE;
    case SE:
      return slot.type == Slot::NW;
    case NE:
      return slot.type == Slot::SW;
    case NW:
      return slot.type == Slot::SE;
    case SW:
      return slot.type == Slot::NE;
    case ALL:
      return true;
    case INVALID:
      LOG(FATAL) << "invalid corner type";
  }
  LOG(FATAL) << "can't reach here";
}

TileOrientation::TileOrientation(int rotation, bool flip,
                                 std::vector<Coord> coords, Coord offset)
    : rotation_(rotation), flip_(flip), coords_(std::move(coords)),
      offset_(std::move(offset)) {
  ComputeSlotsAndCorners();
}

void TileOrientation::ComputeSlotsAndCorners() {
  std::vector<bool> grid(9 * 9, false);
  auto idx = [](int i, int j){ return 9 * i + j; };
  for (const Coord& coord : coords_) {
    grid[idx(coord[0] + 2, coord[1] + 2)] = true;
  }

  for (int i = 1; i < 8; ++i) {
    for (int j = 1; j < 8; ++j) {
      const bool up = grid[idx(i - 1, j)];
      const bool down = grid[idx(i + 1, j)];
      const bool left = grid[idx(i, j - 1)];
      const bool right = grid[idx(i, j + 1)];
      
      if (grid[idx(i, j)]) {
        // Try to classify this tile as a corner.
        Corner::Type type = Corner::INVALID;

        if (coords_.size() == 1) {
          // Degenerate case, 1x1 tile.
          type = Corner::ALL;
        } else if (!up && !left && !right && down) {
          type = Corner::NORTH;
        } else if (!up && !left && right && !down) {
          type = Corner::WEST;
        } else if (!up && left && !right && !down) {
          type = Corner::EAST;
        } else if (up && !left && !right && !down) {
          type = Corner::SOUTH;
        } else if (!up && left && !right && down) {
          type = Corner::NE;
        } else if (!up && !left && right && down) {
          type = Corner::NW;
        } else if (up && left && !right && !down) {
          type = Corner::SE;
        } else if (up && !left && right && !down) {
          type = Corner::SW;
        }
        
        if (type != Corner::INVALID) {
          corners_.push_back(Corner{Coord(i - 2, j - 2), type});
        }
      } else if (!up && !down && !left && !right) {
        // Try to identify this tile as a slot.
        Slot::Type type = Slot::INVALID;
        const bool nw = grid[idx(i - 1, j - 1)];
        const bool ne = grid[idx(i - 1, j + 1)];
        const bool sw = grid[idx(i + 1, j - 1)];
        const bool se = grid[idx(i + 1, j + 1)];

        if (!nw && !ne && sw && se) {
          type = Slot::NORTH;
        } else if (nw && !ne && sw && !se) {
          type = Slot::EAST;
        } else if (!nw && ne && !sw && se) {
          type = Slot::WEST;
        } else if (nw && ne && !sw && !se) {
          type = Slot::SOUTH;
        } else if (nw && !ne && !sw && !se) {
          type = Slot::SE;
        } else if (!nw && ne && !sw && !se) {
          type = Slot::SW;
        } else if (!nw && !ne && sw && !se) {
          type = Slot::NE;
        } else if (!nw && !ne && !sw && se) {
          type = Slot::NW;
        }
        
        if (type != Slot::INVALID) {
          slots_.push_back(Slot{Coord(i - 2, j - 2), type});
        }
      }
    }
  }
}

Tile::Tile(int index, std::vector<std::vector<bool>> blocks)
    : index_(index), blocks_(std::move(blocks)) {
  CHECK_EQ(5, blocks_.size());
  ComputeTransformations();
  ComputeOrientations();
}

void Tile::ComputeTransformations() {
  for (int rotation = 0; rotation < 4; ++rotation) {
    for (bool flip : {true, false}) {
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

void Tile::ComputeOrientations() {
  for (int rotation = 0; rotation < 4; ++rotation) {
    for (bool flip : {false, true}) {
      std::vector<Coord> coords = Transform(rotation, flip);
      // Normalize coordinates so that upper-left is 0,0.
      int8_t min_x = 0;
      int8_t min_y = 0;
      for (const Coord& coord : coords) {
        min_x = std::min(min_x, coord[0]);
        min_y = std::min(min_y, coord[1]);
      }
      for (Coord& coord : coords) {
        coord[0] -= min_x;
        coord[1] -= min_y;
      }

      // If there are no existing orientations, add this one.
      if (orientations_.empty()) {
        orientations_.push_back(
            TileOrientation(rotation, flip, std::move(coords), Coord(0, 0)));
        continue;
      }
      
      // See if we match any existing orientations.
      bool match = false;
      for (const TileOrientation& orientation : orientations_) {
        if (CoordsMatch(coords, orientation.coords())) {
          match = true;
          break;
        }
      }
      if (!match) {
        orientations_.push_back(
            TileOrientation(rotation, flip, std::move(coords),
                            Coord(-min_x, -min_y)));
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
