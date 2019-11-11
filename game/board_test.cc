#include "game/board.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace blokus {
namespace {

using ::testing::Eq;

TEST(BoardTest, IsPossible) {
  Board board;
  // The 1x1 tile should be possible at the start in any orientation, but
  // not anywhere else on the board.
  for (int rotation = 0; rotation < 4; rotation++) {
    for (bool flip : {false, true}) {
      for (uint8_t x = 0; x < Board::kWidth; ++x) {
        for (uint8_t y = 0; y < Board::kHeight; ++y) {
          if (x == 0 && y == 0) {
            EXPECT_TRUE(board.IsPossible({BLUE, 0, {{x, y}, rotation, flip}}));
          } else {
            EXPECT_FALSE(board.IsPossible({BLUE, 0, {{x, y}, rotation, flip}}));
          }
        }
      }
    }
  }
}

}  // namespace
}  // namespace blokus
