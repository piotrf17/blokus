#include "zero/features.h"

#include <algorithm>
#include <random>
#include <set>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "game/board.h"
#include "game/defs.h"
#include "game/game.h"
#include "game/tile.h"

namespace blokus {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::Lt;

constexpr int kRows = Board::kNumRows;
constexpr int kCols = Board::kNumCols;

std::pair<int, int> RowCol(const Coord& c) { return {c.row(), c.col()}; }

TEST(FeaturesTest, FrameColors) {
  EXPECT_THAT((std::vector<Color>{FrameColor(BLUE, 0), FrameColor(BLUE, 1),
                                  FrameColor(BLUE, 2), FrameColor(BLUE, 3)}),
              ElementsAre(BLUE, YELLOW, RED, GREEN));
  EXPECT_THAT((std::vector<Color>{FrameColor(RED, 0), FrameColor(RED, 1),
                                  FrameColor(RED, 2), FrameColor(RED, 3)}),
              ElementsAre(RED, GREEN, BLUE, YELLOW));
  EXPECT_THAT(FrameColor(GREEN, 1), Eq(BLUE));
}

TEST(FeaturesTest, ToFrameMovesStartCornersAround) {
  // Start corners, clockwise from the top left: BLUE, YELLOW, RED, GREEN.
  const std::pair<int, int> kTopLeft = {0, 0};
  const std::pair<int, int> kTopRight = {0, kCols - 1};
  const std::pair<int, int> kBottomRight = {kRows - 1, kCols - 1};
  const std::pair<int, int> kBottomLeft = {kRows - 1, 0};
  EXPECT_THAT(RowCol(ToFrame(BLUE, 0, 0)), Eq(kTopLeft));
  EXPECT_THAT(RowCol(ToFrame(BLUE, 0, kCols - 1)), Eq(kTopRight));
  EXPECT_THAT(RowCol(ToFrame(YELLOW, 0, kCols - 1)), Eq(kTopLeft));
  EXPECT_THAT(RowCol(ToFrame(YELLOW, kRows - 1, kCols - 1)), Eq(kTopRight));
  EXPECT_THAT(RowCol(ToFrame(YELLOW, kRows - 1, 0)), Eq(kBottomRight));
  EXPECT_THAT(RowCol(ToFrame(YELLOW, 0, 0)), Eq(kBottomLeft));
  EXPECT_THAT(RowCol(ToFrame(RED, kRows - 1, kCols - 1)), Eq(kTopLeft));
  EXPECT_THAT(RowCol(ToFrame(RED, 0, 0)), Eq(kBottomRight));
  EXPECT_THAT(RowCol(ToFrame(GREEN, kRows - 1, 0)), Eq(kTopLeft));
  EXPECT_THAT(RowCol(ToFrame(GREEN, 0, 0)), Eq(kTopRight));
  // A cell off the diagonal: (2, 5) for BLUE.
  EXPECT_THAT(RowCol(ToFrame(YELLOW, 2, 5)), Eq(std::make_pair(14, 2)));
  EXPECT_THAT(RowCol(ToFrame(RED, 2, 5)), Eq(std::make_pair(17, 14)));
  EXPECT_THAT(RowCol(ToFrame(GREEN, 2, 5)), Eq(std::make_pair(5, 17)));
}

TEST(FeaturesTest, GlobalOrientations) {
  EXPECT_THAT(GlobalOrientation(0, 0), Eq(0));
  // The 1x1 and the 2x1 have 1 and 2 orientations.
  EXPECT_THAT(GlobalOrientation(1, 0), Eq(1));
  EXPECT_THAT(GlobalOrientation(2, 0), Eq(3));
  const int last = kTiles[kNumTiles - 1].orientations().size() - 1;
  EXPECT_THAT(GlobalOrientation(kNumTiles - 1, last), Eq(kNumOrientations - 1));
}

// The tile and orientation of a global orientation.
std::pair<int, int> TileAndOrientation(int global) {
  for (int tile = 0; tile < kNumTiles; ++tile) {
    const int n = kTiles[tile].orientations().size();
    if (global < GlobalOrientation(tile, 0) + n) {
      return {tile, global - GlobalOrientation(tile, 0)};
    }
  }
  ADD_FAILURE() << "No orientation " << global;
  return {0, 0};
}

// The cells, in the mover's frame, of the placement a policy index stands
// for.
std::set<std::pair<int, int>> PolicyCells(int index) {
  const auto [tile, o] = TileAndOrientation(index / kBoardCells);
  const int row = index % kBoardCells / kCols;
  const int col = index % kCols;
  const TileOrientation& orientation = kTiles[tile].orientations()[o];
  std::set<std::pair<int, int>> cells;
  for (int r = 0; r < orientation.num_rows(); ++r) {
    for (int c = 0; c < orientation.num_cols(); ++c) {
      if (orientation.rows()[r] & (1u << c)) cells.insert({row + r, col + c});
    }
  }
  return cells;
}

// Plays random games, calling `check` on every position before each move.
template <typename Check>
void ForRandomPositions(int num_games, Check check) {
  std::mt19937 rng(1);
  for (int g = 0; g < num_games; ++g) {
    Game game(g % 2 == 0 ? 2 : 4);
    while (!game.Finished()) {
      check(game);
      const std::vector<Move> moves = game.PossibleMoves();
      Move move = Move::EmptyMove(game.current_color());
      if (!moves.empty()) {
        std::uniform_int_distribution<int> pick(0, moves.size() - 1);
        move = moves[pick(rng)];
      }
      ASSERT_TRUE(game.MakeMove(move));
    }
  }
}

// Every legal move maps to a distinct policy index, which stands for the
// same cells as the move, in the mover's frame.
TEST(FeaturesTest, PolicyIndexMatchesCells) {
  int num_checked = 0;
  ForRandomPositions(6, [&](const Game& game) {
    std::set<int> seen;
    for (const Move& move : game.PossibleMoves()) {
      const int index = PolicyIndex(move);
      ASSERT_THAT(index, Lt(kPolicySize));
      ASSERT_TRUE(seen.insert(index).second) << move.DebugString();
      std::set<std::pair<int, int>> cells;
      for (const Coord& cell : move.Cells()) {
        cells.insert(RowCol(ToFrame(move.color, cell.row(), cell.col())));
      }
      ASSERT_THAT(PolicyCells(index), Eq(cells)) << move.DebugString();
      ++num_checked;
    }
  });
  EXPECT_THAT(num_checked, Gt(10000));
}

// The same move in every frame: the 1x1 on each color's start corner is the
// 1x1 at the top left.
TEST(FeaturesTest, OpeningMovesInEveryFrame) {
  const std::pair<int, int> kCorners[] = {
      {0, 0}, {0, kCols - 1}, {kRows - 1, kCols - 1}, {kRows - 1, 0}};
  for (Color color : {BLUE, YELLOW, RED, GREEN}) {
    const auto [row, col] = kCorners[color - BLUE];
    Move move;
    move.color = color;
    move.tile = 0;
    move.placement.coord = Coord(row, col);
    EXPECT_THAT(PolicyIndex(move), Eq(0)) << ColorToString(color);
  }
}

TEST(FeaturesTest, EncodeOpening) {
  Game game(4);
  for (Color mover : {BLUE, YELLOW, RED, GREEN}) {
    ASSERT_THAT(game.current_color(), Eq(mover));
    std::vector<float> planes(kPlanesSize, -1);
    std::vector<float> globals(kNumGlobals, -1);
    EncodeInputs(game, absl::MakeSpan(planes), absl::MakeSpan(globals));
    // Nobody has played, the colors before the mover have passed, and
    // everything is available.
    for (int i = 0; i < 4; ++i) {
      for (int tile = 0; tile < kNumTiles; ++tile) {
        ASSERT_THAT(globals[kNumTiles * i + tile], Eq(1));
      }
      EXPECT_THAT(globals[4 * kNumTiles + i],
                  Eq(FrameColor(mover, i) < mover ? 0 : 1));
    }
    for (int i = 0; i < 4; ++i) {
      for (int cell = 0; cell < kBoardCells; ++cell) {
        ASSERT_THAT(planes[i * kBoardCells + cell], Eq(0));
        ASSERT_THAT(planes[(8 + i) * kBoardCells + cell], Eq(1));
      }
    }
    // Each frame color's only corner is its own start corner.
    const int kCornerCells[] = {0, kCols - 1, kBoardCells - 1,
                                (kRows - 1) * kCols};
    for (int i = 0; i < 4; ++i) {
      for (int cell = 0; cell < kBoardCells; ++cell) {
        ASSERT_THAT(planes[(4 + i) * kBoardCells + cell],
                    Eq(cell == kCornerCells[i] ? 1 : 0))
            << ColorToString(mover) << " frame color " << i << " cell "
            << cell;
      }
    }
    ASSERT_TRUE(game.MakeMove(Move::EmptyMove(mover)));
  }
}

// The planes and globals match the board and game, through ToFrame.
TEST(FeaturesTest, EncodeMatchesGame) {
  std::vector<float> planes(kPlanesSize);
  std::vector<float> globals(kNumGlobals);
  ForRandomPositions(4, [&](const Game& game) {
    EncodeInputs(game, absl::MakeSpan(planes), absl::MakeSpan(globals));
    const Color mover = game.current_color();
    const Board& board = game.board();
    for (int i = 0; i < 4; ++i) {
      const Color color = FrameColor(mover, i);
      for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kCols; ++col) {
          const Coord frame = ToFrame(mover, row, col);
          const int cell = frame.row() * kCols + frame.col();
          const uint32_t bit = 1u << col;
          ASSERT_THAT(planes[i * kBoardCells + cell],
                      Eq(board.At(row, col) == color ? 1 : 0));
          ASSERT_THAT(planes[(4 + i) * kBoardCells + cell],
                      Eq(board.CornerRow(color, row) & bit ? 1 : 0));
          ASSERT_THAT(planes[(8 + i) * kBoardCells + cell],
                      Eq(board.AvailableRow(color, row) & bit ? 1 : 0));
        }
      }
      for (int tile = 0; tile < kNumTiles; ++tile) {
        ASSERT_THAT(globals[kNumTiles * i + tile],
                    Eq(game.IsTileUnplayed(color, tile) ? 1 : 0));
      }
      ASSERT_THAT(globals[4 * kNumTiles + i],
                  Eq(game.HasPassed(color) ? 0 : 1));
    }
  });
}

// The mover's corner plane holds the corners of its legal moves: every legal
// move covers a corner of the mover.
TEST(FeaturesTest, LegalMovesCoverMoverCorners) {
  std::vector<float> planes(kPlanesSize);
  std::vector<float> globals(kNumGlobals);
  ForRandomPositions(2, [&](const Game& game) {
    EncodeInputs(game, absl::MakeSpan(planes), absl::MakeSpan(globals));
    for (const Move& move : game.PossibleMoves()) {
      bool covers_corner = false;
      for (const auto& [row, col] : PolicyCells(PolicyIndex(move))) {
        covers_corner |= planes[4 * kBoardCells + row * kCols + col] == 1;
      }
      ASSERT_TRUE(covers_corner) << move.DebugString();
    }
  });
}

}  // namespace
}  // namespace blokus
