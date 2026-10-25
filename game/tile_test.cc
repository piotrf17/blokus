#include "game/tile.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace blokus {
namespace {

using ::testing::ContainerEq;
using ::testing::Eq;
using ::testing::SizeIs;

// A set of (row, col) cells. Used to compare shapes independent of order.
using CellSet = std::set<std::pair<int, int>>;

CellSet ToCellSet(absl::Span<const Coord> coords, int row_offset = 0,
                  int col_offset = 0) {
  CellSet cells;
  for (const Coord& c : coords) {
    cells.insert({c.row() + row_offset, c.col() + col_offset});
  }
  return cells;
}

bool Contains(const CellSet& cells, int row, int col) {
  return cells.count({row, col}) > 0;
}

// True if (row, col) is edge-adjacent to any cell in `cells`.
bool EdgeAdjacent(const CellSet& cells, int row, int col) {
  return Contains(cells, row - 1, col) || Contains(cells, row + 1, col) ||
         Contains(cells, row, col - 1) || Contains(cells, row, col + 1);
}

// True if (row, col) is diagonally adjacent to any cell in `cells`.
bool DiagonallyAdjacent(const CellSet& cells, int row, int col) {
  return Contains(cells, row - 1, col - 1) ||
         Contains(cells, row - 1, col + 1) ||
         Contains(cells, row + 1, col - 1) || Contains(cells, row + 1, col + 1);
}

// Expected sizes and number of physically distinct orientations, indexed by
// tile. These are fixed properties of the standard Blokus pieces.
constexpr int kExpectedSizes[kNumTiles] = {1, 2, 3, 3, 4, 4, 4, 4, 4, 5, 5,
                                           5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
constexpr int kExpectedNumOrientations[kNumTiles] = {
    1, 2, 4, 2, 1, 4, 2, 8, 4, 8, 4, 4, 8, 4, 2, 8, 4, 4, 8, 1, 8};

TEST(CornerFitsSlotTest, Default) {
  // clang-format off
  bool expected_fit_map[10][9] = {
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
  // clang-format on
  for (int corner = 1; corner <= 9; ++corner) {
    for (int slot = 1; slot <= 8; ++slot) {
      Corner c{{0, 0}, static_cast<Corner::Type>(corner)};
      Slot s{{0, 0}, static_cast<Slot::Type>(slot)};
      EXPECT_THAT(CornerFitsSlot(c, s), Eq(expected_fit_map[corner][slot]))
          << "c: " << corner << " s: " << slot;
    }
  }
}

TEST(TileTest, 1x1) {
  Tile tile = kTiles[0];
  ASSERT_THAT(tile.orientations(), SizeIs(1));

  const TileOrientation& o = tile.orientations()[0];
  ASSERT_THAT(o.corners(), SizeIs(1));
  EXPECT_THAT(o.corners()[0].type, Eq(Corner::ALL));
  EXPECT_THAT(o.corners()[0].c, Eq(Coord(0, 0)));
  ASSERT_THAT(o.slots(), SizeIs(4));
  EXPECT_THAT(o.slots()[0].type, Eq(Corner::NW));
  EXPECT_THAT(o.slots()[0].c, Eq(Coord(-1, -1)));
  EXPECT_THAT(o.slots()[1].type, Eq(Corner::NE));
  EXPECT_THAT(o.slots()[1].c, Eq(Coord(-1, 1)));
  EXPECT_THAT(o.slots()[2].type, Eq(Corner::SW));
  EXPECT_THAT(o.slots()[2].c, Eq(Coord(1, -1)));
  EXPECT_THAT(o.slots()[3].type, Eq(Corner::SE));
  EXPECT_THAT(o.slots()[3].c, Eq(Coord(1, 1)));

  EXPECT_THAT(o.num_cols(), Eq(1));
  ASSERT_THAT(o.num_rows(), Eq(1));
  EXPECT_THAT(o.rows()[0], Eq(0b1));
  EXPECT_THAT(o.expanded_rows()[0], Eq(0b010));
  EXPECT_THAT(o.expanded_rows()[1], Eq(0b111));
  EXPECT_THAT(o.expanded_rows()[2], Eq(0b010));
}

TEST(TileTest, 2x1) {
  Tile tile = kTiles[1];
  ASSERT_THAT(tile.orientations(), SizeIs(2));

  const TileOrientation& o1 = tile.orientations()[0];
  ASSERT_THAT(o1.corners(), SizeIs(2));
  EXPECT_THAT(o1.corners()[0].type, Eq(Corner::WEST));
  EXPECT_THAT(o1.corners()[0].c, Eq(Coord(0, 0)));
  EXPECT_THAT(o1.corners()[1].type, Eq(Corner::EAST));
  EXPECT_THAT(o1.corners()[1].c, Eq(Coord(0, 1)));
  ASSERT_THAT(o1.slots(), SizeIs(4));
  EXPECT_THAT(o1.slots()[0].type, Eq(Corner::NW));
  EXPECT_THAT(o1.slots()[0].c, Eq(Coord(-1, -1)));
  EXPECT_THAT(o1.slots()[1].type, Eq(Corner::NE));
  EXPECT_THAT(o1.slots()[1].c, Eq(Coord(-1, 2)));
  EXPECT_THAT(o1.slots()[2].type, Eq(Corner::SW));
  EXPECT_THAT(o1.slots()[2].c, Eq(Coord(1, -1)));
  EXPECT_THAT(o1.slots()[3].type, Eq(Corner::SE));
  EXPECT_THAT(o1.slots()[3].c, Eq(Coord(1, 2)));

  EXPECT_THAT(o1.num_cols(), Eq(2));
  ASSERT_THAT(o1.num_rows(), Eq(1));
  EXPECT_THAT(o1.rows()[0], Eq(0b11));
  EXPECT_THAT(o1.expanded_rows()[0], Eq(0b0110));
  EXPECT_THAT(o1.expanded_rows()[1], Eq(0b1111));
  EXPECT_THAT(o1.expanded_rows()[2], Eq(0b0110));

  const TileOrientation& o2 = tile.orientations()[1];
  ASSERT_THAT(o2.corners(), SizeIs(2));
  EXPECT_THAT(o2.corners()[0].type, Eq(Corner::NORTH));
  EXPECT_THAT(o2.corners()[0].c, Eq(Coord(0, 0)));
  EXPECT_THAT(o2.corners()[1].type, Eq(Corner::SOUTH));
  EXPECT_THAT(o2.corners()[1].c, Eq(Coord(1, 0)));
  ASSERT_THAT(o2.slots(), SizeIs(4));
  EXPECT_THAT(o2.slots()[0].type, Eq(Corner::NW));
  EXPECT_THAT(o2.slots()[0].c, Eq(Coord(-1, -1)));
  EXPECT_THAT(o2.slots()[1].type, Eq(Corner::NE));
  EXPECT_THAT(o2.slots()[1].c, Eq(Coord(-1, 1)));
  EXPECT_THAT(o2.slots()[2].type, Eq(Corner::SW));
  EXPECT_THAT(o2.slots()[2].c, Eq(Coord(2, -1)));
  EXPECT_THAT(o2.slots()[3].type, Eq(Corner::SE));
  EXPECT_THAT(o2.slots()[3].c, Eq(Coord(2, 1)));

  EXPECT_THAT(o2.num_cols(), Eq(1));
  ASSERT_THAT(o2.num_rows(), Eq(2));
  EXPECT_THAT(o2.rows()[0], Eq(0b1));
  EXPECT_THAT(o2.rows()[1], Eq(0b1));
  EXPECT_THAT(o2.expanded_rows()[0], Eq(0b010));
  EXPECT_THAT(o2.expanded_rows()[1], Eq(0b111));
  EXPECT_THAT(o2.expanded_rows()[2], Eq(0b111));
  EXPECT_THAT(o2.expanded_rows()[3], Eq(0b010));
}

TEST(TileTest, C) {
  Tile tile = kTiles[17];
  ASSERT_THAT(tile.orientations(), SizeIs(4));

  const TileOrientation& o1 = tile.orientations()[0];
  ASSERT_THAT(o1.corners(), SizeIs(4));
  EXPECT_THAT(o1.corners()[0].type, Eq(Corner::NW));
  EXPECT_THAT(o1.corners()[0].c, Eq(Coord(0, 0)));
  EXPECT_THAT(o1.corners()[1].type, Eq(Corner::EAST));
  EXPECT_THAT(o1.corners()[1].c, Eq(Coord(0, 1)));
  EXPECT_THAT(o1.corners()[2].type, Eq(Corner::SW));
  EXPECT_THAT(o1.corners()[2].c, Eq(Coord(2, 0)));
  EXPECT_THAT(o1.corners()[3].type, Eq(Corner::EAST));
  EXPECT_THAT(o1.corners()[3].c, Eq(Coord(2, 1)));
  ASSERT_THAT(o1.slots(), SizeIs(5));
  EXPECT_THAT(o1.slots()[0].type, Eq(Corner::NW));
  EXPECT_THAT(o1.slots()[0].c, Eq(Coord(-1, -1)));
  EXPECT_THAT(o1.slots()[1].type, Eq(Corner::NE));
  EXPECT_THAT(o1.slots()[1].c, Eq(Coord(-1, 2)));
  EXPECT_THAT(o1.slots()[2].type, Eq(Corner::EAST));
  EXPECT_THAT(o1.slots()[2].c, Eq(Coord(1, 2)));
  EXPECT_THAT(o1.slots()[3].type, Eq(Corner::SW));
  EXPECT_THAT(o1.slots()[3].c, Eq(Coord(3, -1)));
  EXPECT_THAT(o1.slots()[4].type, Eq(Corner::SE));
  EXPECT_THAT(o1.slots()[4].c, Eq(Coord(3, 2)));

  EXPECT_THAT(o1.num_cols(), Eq(2));
  ASSERT_THAT(o1.num_rows(), Eq(3));
  EXPECT_THAT(o1.rows()[0], Eq(0b11));
  EXPECT_THAT(o1.rows()[1], Eq(0b01));
  EXPECT_THAT(o1.rows()[2], Eq(0b11));
  EXPECT_THAT(o1.expanded_rows()[0], Eq(0b0110));
  EXPECT_THAT(o1.expanded_rows()[1], Eq(0b1111));
  EXPECT_THAT(o1.expanded_rows()[2], Eq(0b0111));
  EXPECT_THAT(o1.expanded_rows()[3], Eq(0b1111));
  EXPECT_THAT(o1.expanded_rows()[4], Eq(0b0110));
}

TEST(TileTest, Cross) {
  Tile tile = kTiles[19];
  ASSERT_THAT(tile.orientations(), SizeIs(1));

  const TileOrientation& o = tile.orientations()[0];
  ASSERT_THAT(o.corners(), SizeIs(4));
  EXPECT_THAT(o.corners()[0].type, Eq(Corner::NORTH));
  EXPECT_THAT(o.corners()[0].c, Eq(Coord(0, 1)));
  EXPECT_THAT(o.corners()[1].type, Eq(Corner::WEST));
  EXPECT_THAT(o.corners()[1].c, Eq(Coord(1, 0)));
  EXPECT_THAT(o.corners()[2].type, Eq(Corner::EAST));
  EXPECT_THAT(o.corners()[2].c, Eq(Coord(1, 2)));
  EXPECT_THAT(o.corners()[3].type, Eq(Corner::SOUTH));
  EXPECT_THAT(o.corners()[3].c, Eq(Coord(2, 1)));
  ASSERT_THAT(o.slots(), SizeIs(8));
  EXPECT_THAT(o.slots()[0].type, Eq(Corner::NW));
  EXPECT_THAT(o.slots()[0].c, Eq(Coord(-1, 0)));
  EXPECT_THAT(o.slots()[1].type, Eq(Corner::NE));
  EXPECT_THAT(o.slots()[1].c, Eq(Coord(-1, 2)));
  EXPECT_THAT(o.slots()[2].type, Eq(Corner::NW));
  EXPECT_THAT(o.slots()[2].c, Eq(Coord(0, -1)));
  EXPECT_THAT(o.slots()[3].type, Eq(Corner::NE));
  EXPECT_THAT(o.slots()[3].c, Eq(Coord(0, 3)));
  EXPECT_THAT(o.slots()[4].type, Eq(Corner::SW));
  EXPECT_THAT(o.slots()[4].c, Eq(Coord(2, -1)));
  EXPECT_THAT(o.slots()[5].type, Eq(Corner::SE));
  EXPECT_THAT(o.slots()[5].c, Eq(Coord(2, 3)));
  EXPECT_THAT(o.slots()[6].type, Eq(Corner::SW));
  EXPECT_THAT(o.slots()[6].c, Eq(Coord(3, 0)));
  EXPECT_THAT(o.slots()[7].type, Eq(Corner::SE));
  EXPECT_THAT(o.slots()[7].c, Eq(Coord(3, 2)));

  EXPECT_THAT(o.num_cols(), Eq(3));
  ASSERT_THAT(o.num_rows(), Eq(3));
  EXPECT_THAT(o.rows()[0], Eq(0b010));
  EXPECT_THAT(o.rows()[1], Eq(0b111));
  EXPECT_THAT(o.rows()[2], Eq(0b010));
}

TEST(TileTest, SizesMatchStandardPieces) {
  int total = 0;
  for (int t = 0; t < kNumTiles; ++t) {
    EXPECT_THAT(kTiles[t].Size(), Eq(kExpectedSizes[t])) << "tile " << t;
    EXPECT_THAT(kTiles[t].index(), Eq(t));
    total += kTiles[t].Size();
  }
  EXPECT_THAT(total, Eq(89));
}

TEST(TileTest, NumOrientationsMatchStandardPieces) {
  int total = 0;
  for (int t = 0; t < kNumTiles; ++t) {
    EXPECT_THAT(kTiles[t].orientations(), SizeIs(kExpectedNumOrientations[t]))
        << "tile " << t;
    total += kTiles[t].orientations().size();
  }
  EXPECT_THAT(total, Eq(91));
}

TEST(TileTest, AllTilesAreDistinctShapes) {
  // No orientation of one tile may match any orientation of another tile.
  std::map<CellSet, int> seen;
  for (int t = 0; t < kNumTiles; ++t) {
    for (const TileOrientation& o : kTiles[t].orientations()) {
      auto [it, inserted] = seen.emplace(ToCellSet(o.coords()), t);
      EXPECT_TRUE(inserted || it->second == t)
          << "tiles " << it->second << " and " << t << " share a shape";
    }
  }
}

TEST(TileTest, OrientationsAreDistinctAndCoverAllTransforms) {
  for (int t = 0; t < kNumTiles; ++t) {
    const Tile& tile = kTiles[t];
    std::set<CellSet> orientation_shapes;
    std::set<std::pair<int, bool>> orientation_keys;
    for (const TileOrientation& o : tile.orientations()) {
      orientation_shapes.insert(ToCellSet(o.coords()));
      orientation_keys.insert({o.rotation(), o.flip()});
    }
    // Each orientation is a distinct shape with a distinct (rotation, flip).
    EXPECT_THAT(orientation_shapes, SizeIs(tile.orientations().size()))
        << "tile " << t;
    EXPECT_THAT(orientation_keys, SizeIs(tile.orientations().size()))
        << "tile " << t;

    // Every one of the 8 transforms is one of the orientations.
    for (int rotation = 0; rotation < 4; ++rotation) {
      for (bool flip : {false, true}) {
        const std::vector<Coord>& coords = tile.Transform(rotation, flip);
        int min_row = 0, min_col = 0;
        for (const Coord& c : coords) {
          min_row = std::min<int>(min_row, c.row());
          min_col = std::min<int>(min_col, c.col());
        }
        EXPECT_TRUE(
            orientation_shapes.count(ToCellSet(coords, -min_row, -min_col)))
            << "tile " << t << " rotation " << rotation << " flip " << flip;
      }
    }
  }
}

TEST(TileTest, OrientationCoordsAreNormalizedTransforms) {
  for (int t = 0; t < kNumTiles; ++t) {
    const Tile& tile = kTiles[t];
    for (const TileOrientation& o : tile.orientations()) {
      SCOPED_TRACE(testing::Message() << "tile " << t << " rotation "
                                      << o.rotation() << " flip " << o.flip());
      // Upper-left of the bounding box is (0, 0).
      int min_row = 100, min_col = 100, max_row = -1, max_col = -1;
      for (const Coord& c : o.coords()) {
        min_row = std::min<int>(min_row, c.row());
        min_col = std::min<int>(min_col, c.col());
        max_row = std::max<int>(max_row, c.row());
        max_col = std::max<int>(max_col, c.col());
      }
      EXPECT_THAT(min_row, Eq(0));
      EXPECT_THAT(min_col, Eq(0));
      EXPECT_THAT(o.num_rows(), Eq(max_row + 1));
      EXPECT_THAT(o.num_cols(), Eq(max_col + 1));

      // The raw transform equals the normalized coords shifted by -offset.
      // Board relies on this: the upper-left of a placed tile on the board is
      // `placement.coord - offset`.
      EXPECT_THAT(ToCellSet(tile.Transform(o.rotation(), o.flip())),
                  ContainerEq(ToCellSet(o.coords(), -o.offset().row(),
                                        -o.offset().col())));
    }
  }
}

TEST(TileTest, RowBitmapsMatchCoords) {
  for (int t = 0; t < kNumTiles; ++t) {
    for (const TileOrientation& o : kTiles[t].orientations()) {
      SCOPED_TRACE(testing::Message() << "tile " << t << " rotation "
                                      << o.rotation() << " flip " << o.flip());
      const CellSet cells = ToCellSet(o.coords());
      ASSERT_THAT(o.rows(), SizeIs(5));
      for (int r = 0; r < 5; ++r) {
        uint32_t expected = 0;
        for (int c = 0; c < 5; ++c) {
          if (Contains(cells, r, c)) expected |= 1u << c;
        }
        EXPECT_THAT(o.rows()[r], Eq(expected)) << "row " << r;
      }

      // Expanded rows are shifted by one row and one column, and include all
      // cells that are in or edge-adjacent to the tile.
      ASSERT_THAT(o.expanded_rows(), SizeIs(7));
      for (int r = 0; r < 7; ++r) {
        uint32_t expected = 0;
        for (int c = 0; c < 7; ++c) {
          if (Contains(cells, r - 1, c - 1) ||
              EdgeAdjacent(cells, r - 1, c - 1)) {
            expected |= 1u << c;
          }
        }
        EXPECT_THAT(o.expanded_rows()[r], Eq(expected)) << "row " << r;
      }
    }
  }
}

// Expected corner type for a tile cell, given which of its edge neighbors are
// part of the tile. Returns INVALID if the cell can't be a corner.
Corner::Type ExpectedCornerType(const CellSet& cells, int row, int col) {
  if (cells.size() == 1) return Corner::ALL;
  const bool up = Contains(cells, row - 1, col);
  const bool down = Contains(cells, row + 1, col);
  const bool left = Contains(cells, row, col - 1);
  const bool right = Contains(cells, row, col + 1);
  // Corner type names the direction the cell points, e.g. a NORTH corner is
  // the northern tip of the tile and only has a neighbor below it.
  if (!up && !left && !right && down) return Corner::NORTH;
  if (up && !left && !right && !down) return Corner::SOUTH;
  if (!up && !left && right && !down) return Corner::WEST;
  if (!up && left && !right && !down) return Corner::EAST;
  if (!up && left && !right && down) return Corner::NE;
  if (!up && !left && right && down) return Corner::NW;
  if (up && left && !right && !down) return Corner::SE;
  if (up && !left && right && !down) return Corner::SW;
  return Corner::INVALID;
}

TEST(TileTest, CornersMatchBruteForce) {
  for (int t = 0; t < kNumTiles; ++t) {
    for (const TileOrientation& o : kTiles[t].orientations()) {
      SCOPED_TRACE(testing::Message() << "tile " << t << " rotation "
                                      << o.rotation() << " flip " << o.flip());
      const CellSet cells = ToCellSet(o.coords());

      // A cell can touch another piece of the same color only if one of its
      // diagonal neighbors is free and not edge-adjacent to the tile.
      std::map<std::pair<int, int>, Corner::Type> expected;
      for (const auto& [r, c] : cells) {
        bool has_free_diagonal = false;
        for (int dr : {-1, 1}) {
          for (int dc : {-1, 1}) {
            if (!Contains(cells, r + dr, c + dc) &&
                !EdgeAdjacent(cells, r + dr, c + dc)) {
              has_free_diagonal = true;
            }
          }
        }
        const Corner::Type type = ExpectedCornerType(cells, r, c);
        EXPECT_THAT(type != Corner::INVALID, Eq(has_free_diagonal))
            << "cell (" << r << "," << c << ")";
        if (has_free_diagonal) expected[{r, c}] = type;
      }

      std::map<std::pair<int, int>, Corner::Type> actual;
      for (const Corner& corner : o.corners()) {
        const std::pair<int, int> cell(corner.c.row(), corner.c.col());
        EXPECT_TRUE(actual.emplace(cell, corner.type).second)
            << "duplicate corner at " << corner.c;
      }
      EXPECT_THAT(actual, ContainerEq(expected));
    }
  }
}

TEST(TileTest, SlotsMatchBruteForce) {
  for (int t = 0; t < kNumTiles; ++t) {
    for (const TileOrientation& o : kTiles[t].orientations()) {
      SCOPED_TRACE(testing::Message() << "tile " << t << " rotation "
                                      << o.rotation() << " flip " << o.flip());
      const CellSet cells = ToCellSet(o.coords());

      // A slot is an empty cell that is diagonally adjacent to the tile, but
      // not edge-adjacent to it. Its type is the direction from the tile to
      // the slot, e.g. a NORTH slot is above the tile.
      std::map<std::pair<int, int>, Slot::Type> expected;
      for (int r = -1; r <= 5; ++r) {
        for (int c = -1; c <= 5; ++c) {
          if (Contains(cells, r, c) || EdgeAdjacent(cells, r, c) ||
              !DiagonallyAdjacent(cells, r, c)) {
            continue;
          }
          const bool nw = Contains(cells, r - 1, c - 1);
          const bool ne = Contains(cells, r - 1, c + 1);
          const bool sw = Contains(cells, r + 1, c - 1);
          const bool se = Contains(cells, r + 1, c + 1);
          Slot::Type type = Slot::INVALID;
          if (!nw && !ne && sw && se) type = Slot::NORTH;
          if (nw && ne && !sw && !se) type = Slot::SOUTH;
          if (!nw && ne && !sw && se) type = Slot::WEST;
          if (nw && !ne && sw && !se) type = Slot::EAST;
          if (nw && !ne && !sw && !se) type = Slot::SE;
          if (!nw && ne && !sw && !se) type = Slot::SW;
          if (!nw && !ne && sw && !se) type = Slot::NE;
          if (!nw && !ne && !sw && se) type = Slot::NW;
          EXPECT_THAT(type, testing::Ne(Slot::INVALID))
              << "unclassifiable slot at (" << r << "," << c << ")";
          expected[{r, c}] = type;
        }
      }

      std::map<std::pair<int, int>, Slot::Type> actual;
      for (const Slot& slot : o.slots()) {
        const std::pair<int, int> cell(slot.c.row(), slot.c.col());
        EXPECT_TRUE(actual.emplace(cell, slot.type).second)
            << "duplicate slot at " << slot.c;
      }
      EXPECT_THAT(actual, ContainerEq(expected));
    }
  }
}

TEST(CornerFitsSlotTest, MatchesGeometry) {
  // Derive the fit table from first principles. Put the slot at (0, 0). The
  // slot type determines which diagonal cells hold the existing piece. The
  // corner type determines which edge neighbors of (0, 0) the new piece
  // occupies. The corner fits if none of those neighbors is edge-adjacent to
  // the existing piece.
  const std::map<Slot::Type, CellSet> slot_diagonals = {
      {Slot::NORTH, {{1, -1}, {1, 1}}},
      {Slot::SOUTH, {{-1, -1}, {-1, 1}}},
      {Slot::WEST, {{-1, 1}, {1, 1}}},
      {Slot::EAST, {{-1, -1}, {1, -1}}},
      {Slot::SE, {{-1, -1}}},
      {Slot::SW, {{-1, 1}}},
      {Slot::NE, {{1, -1}}},
      {Slot::NW, {{1, 1}}},
  };
  const std::map<Corner::Type, CellSet> corner_neighbors = {
      {Corner::NORTH, {{1, 0}}},
      {Corner::SOUTH, {{-1, 0}}},
      {Corner::WEST, {{0, 1}}},
      {Corner::EAST, {{0, -1}}},
      {Corner::NE, {{0, -1}, {1, 0}}},
      {Corner::NW, {{0, 1}, {1, 0}}},
      {Corner::SE, {{-1, 0}, {0, -1}}},
      {Corner::SW, {{-1, 0}, {0, 1}}},
      {Corner::ALL, {}},
  };
  for (const auto& [slot_type, existing] : slot_diagonals) {
    for (const auto& [corner_type, neighbors] : corner_neighbors) {
      bool fits = true;
      for (const auto& [r, c] : neighbors) {
        if (EdgeAdjacent(existing, r, c)) fits = false;
      }
      EXPECT_THAT(
          CornerFitsSlot(Corner{{0, 0}, corner_type}, Slot{{0, 0}, slot_type}),
          Eq(fits))
          << "corner " << corner_type << " slot " << slot_type;
    }
  }
}

TEST(TileTest, CanonicalizePreservesPlacedCells) {
  for (int t = 0; t < kNumTiles; ++t) {
    const Tile& tile = kTiles[t];
    std::set<std::pair<int, bool>> orientation_keys;
    for (const TileOrientation& o : tile.orientations()) {
      orientation_keys.insert({o.rotation(), o.flip()});
    }
    for (int rotation = 0; rotation < 4; ++rotation) {
      for (bool flip : {false, true}) {
        SCOPED_TRACE(testing::Message() << "tile " << t << " rotation "
                                        << rotation << " flip " << flip);
        const Placement placement{Coord(7, 9), rotation, flip};
        const Placement canonical = tile.Canonicalize(placement);

        // The canonical placement uses one of the tile's orientations.
        EXPECT_TRUE(
            orientation_keys.count({canonical.rotation, canonical.flip}));

        // It covers exactly the same cells on the board.
        EXPECT_THAT(
            ToCellSet(tile.Transform(canonical.rotation, canonical.flip),
                      canonical.coord.row(), canonical.coord.col()),
            ContainerEq(ToCellSet(tile.Transform(rotation, flip),
                                  placement.coord.row(),
                                  placement.coord.col())));

        // Canonicalizing is idempotent.
        EXPECT_TRUE(tile.Canonicalize(canonical) == canonical);
      }
    }
  }
}

}  // namespace
}  // namespace blokus
