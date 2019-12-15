#include "game/tile.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace blokus {
namespace {

using ::testing::Eq;
using ::testing::SizeIs;

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
}

}  // namespace
}  // namespace blokus
