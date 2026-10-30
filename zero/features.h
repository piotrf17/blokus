#ifndef BLOKUS_ZERO_FEATURES_H_
#define BLOKUS_ZERO_FEATURES_H_

#include "absl/types/span.h"

#include "game/board.h"
#include "game/defs.h"
#include "game/game.h"

namespace blokus {

// The BlokusZero network's inputs and policy layout. These must match
// zero/train/model.py.
//
// The network sees the board in the frame of the color to move (the mover):
// rotated by a multiple of 90 degrees so that the mover's start corner is at
// the top left. Colors go clockwise from the top left in turn order, so in
// every frame the next color starts at the top right, the partner (the
// mover's other color in a 2-player game) at the bottom right, and the
// previous color at the bottom left. Frame color i is the i-th color in turn
// order starting from the mover.

inline constexpr int kBoardCells = Board::kNumRows * Board::kNumCols;

// Input planes, each kNumRows x kNumCols in the mover's frame. Plane
// 4 * kind + i is for frame color i, where kind is:
//   0: cells it occupies,
//   1: its corners (Board::CornerRow),
//   2: cells available to it (Board::AvailableRow).
inline constexpr int kNumPlanes = 12;
inline constexpr int kPlanesSize = kNumPlanes * kBoardCells;

// Global inputs: 21 * i + tile is 1 if frame color i hasn't played `tile`,
// and 84 + i is 1 if frame color i hasn't passed.
inline constexpr int kNumGlobals = 88;

// The orientations of all tiles: tile t's orientations, in
// Tile::orientations() order, come after those of tiles 0 to t - 1.
inline constexpr int kNumOrientations = 91;

// One policy logit per (orientation, cell): placing the orientation with the
// top-left corner of its bounding box on the cell, in the mover's frame. The
// logit for orientation o at (row, col) has index
// o * kBoardCells + row * kNumCols + col. Most are never legal.
inline constexpr int kPolicySize = kNumOrientations * kBoardCells;

// Win, draw and loss.
inline constexpr int kNumOutcomes = 3;

// Returns frame color `i` (0 to 3) for `mover`.
Color FrameColor(Color mover, int i);

// Returns board cell (row, col) in `mover`'s frame.
Coord ToFrame(Color mover, int row, int col);

// Returns the index among all orientations (see kNumOrientations) of `tile`'s
// orientation `orientation`, an index into Tile::orientations().
int GlobalOrientation(int tile, int orientation);

// Returns the policy index of `move`, in the frame of `move.color`. The move
// must not be a pass, and its placement must be canonical.
int PolicyIndex(const Move& move);

// Writes the network inputs for `game`, in the frame of its current color.
// `planes` must have kPlanesSize floats ([plane][row][col]) and `globals`
// kNumGlobals floats.
void EncodeInputs(const Game& game, absl::Span<float> planes,
                  absl::Span<float> globals);

}  // namespace blokus

#endif
