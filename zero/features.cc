#include "zero/features.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"

#include "game/tile.h"

namespace blokus {
namespace {

constexpr int kRows = Board::kNumRows;
constexpr int kCols = Board::kNumCols;
static_assert(kRows == kCols, "Rotating the board needs a square board");

// An orientation's shape: its size and its num_rows rows, as in
// TileOrientation.
struct Shape {
  int num_rows;
  int num_cols;
  std::vector<uint32_t> rows;

  bool operator==(const Shape&) const = default;
};

Shape ShapeOf(const TileOrientation& orientation) {
  // rows() may have unused rows at the end.
  const absl::Span<const uint32_t> rows =
      orientation.rows().first(orientation.num_rows());
  return {orientation.num_rows(), orientation.num_cols(),
          std::vector<uint32_t>(rows.begin(), rows.end())};
}

// Rotates `shape` by 90 degrees counter-clockwise, like ToFrame does for one
// rotation step: block (r, c) moves to (num_cols - 1 - c, r).
Shape RotateCounterClockwise(const Shape& shape) {
  Shape rotated = {shape.num_cols, shape.num_rows,
                   std::vector<uint32_t>(shape.num_cols, 0)};
  for (int r = 0; r < shape.num_rows; ++r) {
    for (int c = 0; c < shape.num_cols; ++c) {
      if (shape.rows[r] & (1u << c)) {
        rotated.rows[shape.num_cols - 1 - c] |= 1u << r;
      }
    }
  }
  return rotated;
}

struct OrientationTables {
  // Indexed by tile: the global index of its first orientation.
  std::array<int, kNumTiles + 1> first;
  // Indexed by number of counter-clockwise rotations, then global
  // orientation: the global orientation of the rotated shape.
  std::array<std::array<int, kNumOrientations>, 4> rotated;
};

const OrientationTables& Tables() {
  static const OrientationTables* const tables = [] {
    auto* tables = new OrientationTables;
    int next = 0;
    for (int tile = 0; tile < kNumTiles; ++tile) {
      tables->first[tile] = next;
      next += kTiles[tile].orientations().size();
    }
    tables->first[kNumTiles] = next;
    CHECK_EQ(next, kNumOrientations);

    for (int tile = 0; tile < kNumTiles; ++tile) {
      const std::vector<TileOrientation>& orientations =
          kTiles[tile].orientations();
      for (int o = 0; o < static_cast<int>(orientations.size()); ++o) {
        Shape shape = ShapeOf(orientations[o]);
        for (int k = 0; k < 4; ++k) {
          const auto it = std::find_if(
              orientations.begin(), orientations.end(),
              [&](const TileOrientation& other) {
                return ShapeOf(other) == shape;
              });
          CHECK(it != orientations.end()) << "tile " << tile;
          tables->rotated[k][tables->first[tile] + o] =
              tables->first[tile] + (it - orientations.begin());
          shape = RotateCounterClockwise(shape);
        }
      }
    }
    return tables;
  }();
  return *tables;
}

// The number of counter-clockwise rotations that take `mover`'s start corner
// to the top left.
int NumRotations(Color mover) { return mover - BLUE; }

int OrientationIndex(const Move& move) {
  const std::vector<TileOrientation>& orientations =
      kTiles[move.tile].orientations();
  for (int o = 0; o < static_cast<int>(orientations.size()); ++o) {
    if (orientations[o].rotation() == move.placement.rotation &&
        orientations[o].flip() == move.placement.flip) {
      return o;
    }
  }
  LOG(FATAL) << "Not a canonical placement: " << move.DebugString();
}

}  // namespace

Color FrameColor(Color mover, int i) {
  return static_cast<Color>(BLUE + (mover - BLUE + i) % 4);
}

Coord ToFrame(Color mover, int row, int col) {
  for (int k = NumRotations(mover); k > 0; --k) {
    const int new_row = kCols - 1 - col;
    col = row;
    row = new_row;
  }
  return Coord(row, col);
}

int GlobalOrientation(int tile, int orientation) {
  return Tables().first[tile] + orientation;
}

int PolicyIndex(const Move& move) {
  CHECK_NE(move.tile, -1) << "A pass has no policy index";
  const int o = OrientationIndex(move);
  const TileOrientation& orientation = kTiles[move.tile].orientations()[o];
  // The bounding box, rotated a step at a time like the cells in ToFrame.
  int row = move.placement.coord.row() - orientation.offset().row();
  int col = move.placement.coord.col() - orientation.offset().col();
  int num_rows = orientation.num_rows();
  int num_cols = orientation.num_cols();
  const int k = NumRotations(move.color);
  for (int i = 0; i < k; ++i) {
    const int new_row = kCols - col - num_cols;
    col = row;
    row = new_row;
    std::swap(num_rows, num_cols);
  }
  const int rotated = Tables().rotated[k][GlobalOrientation(move.tile, o)];
  return rotated * kBoardCells + row * kCols + col;
}

void EncodeInputs(const Game& game, absl::Span<float> planes,
                  absl::Span<float> globals) {
  CHECK_EQ(planes.size(), kPlanesSize);
  CHECK_EQ(globals.size(), kNumGlobals);
  const Color mover = game.current_color();
  const Board& board = game.board();
  std::fill(planes.begin(), planes.end(), 0.0f);

  // The frame index of each color.
  std::array<int, 5> frame_index = {};
  for (int i = 0; i < 4; ++i) frame_index[FrameColor(mover, i)] = i;

  for (int row = 0; row < kRows; ++row) {
    for (int col = 0; col < kCols; ++col) {
      const Coord frame = ToFrame(mover, row, col);
      const int cell = frame.row() * kCols + frame.col();
      const Color occupant = board.At(row, col);
      if (occupant != INVALID) {
        planes[frame_index[occupant] * kBoardCells + cell] = 1;
      }
      const uint32_t bit = 1u << col;
      for (int i = 0; i < 4; ++i) {
        const Color color = FrameColor(mover, i);
        if (board.CornerRow(color, row) & bit) {
          planes[(4 + i) * kBoardCells + cell] = 1;
        }
        if (board.AvailableRow(color, row) & bit) {
          planes[(8 + i) * kBoardCells + cell] = 1;
        }
      }
    }
  }

  for (int i = 0; i < 4; ++i) {
    const Color color = FrameColor(mover, i);
    for (int tile = 0; tile < kNumTiles; ++tile) {
      globals[kNumTiles * i + tile] = game.IsTileUnplayed(color, tile);
    }
    globals[4 * kNumTiles + i] = !game.HasPassed(color);
  }
}

}  // namespace blokus
