#include "game/defs.h"

#include <sstream>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace blokus {
namespace {

using ::testing::Eq;
using ::testing::Ne;

TEST(ColorTest, NextColorCyclesInTurnOrder) {
  EXPECT_THAT(NextColor(BLUE), Eq(YELLOW));
  EXPECT_THAT(NextColor(YELLOW), Eq(RED));
  EXPECT_THAT(NextColor(RED), Eq(GREEN));
  EXPECT_THAT(NextColor(GREEN), Eq(BLUE));
}

TEST(ColorTest, ColorToString) {
  EXPECT_THAT(ColorToString(BLUE), Eq("blue"));
  EXPECT_THAT(ColorToString(YELLOW), Eq("yellow"));
  EXPECT_THAT(ColorToString(RED), Eq("red"));
  EXPECT_THAT(ColorToString(GREEN), Eq("green"));
}

TEST(ColorDeathTest, InvalidColorDies) {
  EXPECT_DEATH(NextColor(INVALID), "Unknown color");
  EXPECT_DEATH(ColorToString(INVALID), "Unknown color");
  EXPECT_DEATH(AnsiColor(INVALID), "Unknown color");
}

TEST(CoordTest, DefaultIsOrigin) {
  Coord c;
  EXPECT_THAT(c.row(), Eq(0));
  EXPECT_THAT(c.col(), Eq(0));
}

TEST(CoordTest, RowAndColAccessors) {
  Coord c(3, -2);
  EXPECT_THAT(c.row(), Eq(3));
  EXPECT_THAT(c.col(), Eq(-2));
  EXPECT_THAT(c[0], Eq(3));
  EXPECT_THAT(c[1], Eq(-2));
  c[1] = 7;
  EXPECT_THAT(c.col(), Eq(7));
}

TEST(CoordTest, Equality) {
  EXPECT_THAT(Coord(1, 2), Eq(Coord(1, 2)));
  EXPECT_FALSE(Coord(1, 2) == Coord(2, 1));
  EXPECT_FALSE(Coord(1, 2) == Coord(1, 3));
}

TEST(CoordTest, StreamOutput) {
  std::ostringstream os;
  os << Coord(1, -12);
  EXPECT_THAT(os.str(), Eq("( 1,-12)"));
}

TEST(PlacementTest, Equality) {
  const Placement p{Coord(4, 5), 2, true};
  EXPECT_TRUE(p == (Placement{Coord(4, 5), 2, true}));
  EXPECT_FALSE(p == (Placement{Coord(4, 6), 2, true}));
  EXPECT_FALSE(p == (Placement{Coord(4, 5), 3, true}));
  EXPECT_FALSE(p == (Placement{Coord(4, 5), 2, false}));
}

TEST(PlacementTest, DebugString) {
  const Placement p{Coord(4, 15), 3, true};
  EXPECT_THAT(p.DebugString(), Eq("( 4,15) 3 1"));
  EXPECT_THAT(Placement().DebugString(), Ne(p.DebugString()));
}

}  // namespace
}  // namespace blokus
