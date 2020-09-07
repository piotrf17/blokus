#include "game/tile.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace blokus {
namespace {

using ::testing::Eq;
using ::testing::SizeIs;

TEST(CornerFitsSlotTest, Default) {
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

}  // namespace
}  // namespace blokus
