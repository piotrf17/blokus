#include "game/board.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

#define ANSI_COLOR_RESET   "\x1b[0m"

namespace blokus {

namespace {

// Returns all translated, rotated, and flipped coordinates for the placed
// tile.
// TODO(piotrf): consider exposing this as a public api.
std::vector<Coord> PlacedTile(const Tile& tile, const Placement& placement) {
  std::vector<Coord> coors = tile.Transform(placement.rotation, placement.flip);
  for (auto& coor : coors) {
    coor[0] += placement.coord[0];
    coor[1] += placement.coord[1];
  }
  return coors;
}

const TileOrientation& OrientationForMove(const Move& move) {
  for (const TileOrientation& o : kTiles[move.tile].orientations()) {
    if (o.rotation() == move.placement.rotation &&
        o.flip() == move.placement.flip) {
      return o;
    }
  }
  LOG(FATAL) << "No orientation found for move: " << move.DebugString();
}

constexpr int RC(int row, int col) {
  return Board::kNumCols * row + col;
}

int PlacementHash(const Placement& p) {
  return RC(p.coord.row(), p.coord.col()) * 8 + p.rotation * 2 + p.flip;
}

// One more than the largest value PlacementHash can return.
constexpr int kNumPlacementHashes =
    RC(Board::kNumRows - 1, Board::kNumCols - 1) * 8 + 3 * 2 + 1 + 1;

// Returns the squares of `slot`'s SlotWindow that are set in `board_rows` (in
// the layout of Board::available_), as a bitmap like
// Tile::CornerPlacement::mask. Squares off the board are unset.
uint64_t WindowAround(const Slot& slot, const uint32_t board_rows[]) {
  constexpr uint32_t kRowMask = (1u << Board::kNumCols) - 1;
  const SlotWindow window = GetSlotWindow(slot.type);
  const int col = slot.c.col() + window.col;
  uint64_t bits = 0;
  for (int i = 0; i < 8; ++i) {
    const int row = slot.c.row() + window.row + i;
    if (row < 0 || row >= Board::kNumRows) continue;
    const uint32_t board_row = board_rows[row] & kRowMask;
    const uint32_t window_row =
        col >= 0 ? board_row >> col : board_row << -col;
    bits |= uint64_t{window_row & 0xff} << (i * 8);
  }
  return bits;
}

}  // namespace

Move Move::EmptyMove(Color color) {
  Move move;
  move.color = color;
  move.tile = -1;
  return move;
}

std::vector<Coord> Move::Cells() const {
  if (tile == -1) return {};
  return PlacedTile(kTiles[tile], placement);
}

std::string Move::DebugString() const {
  if (tile == -1) {
    return absl::StrCat(ColorToString(color), " played pass");
  } else {
    return absl::StrCat(ColorToString(color), " played tile ", tile, " at ",
                        placement.DebugString());
  }
}

Board::Board() {
  // Initially, there are no pieces on the board.
  for (int r = 0; r < kNumRows; ++r) {
    for (int c = 0; c < kNumCols; ++c) {
      pieces_[r][c] = INVALID;
    }
  }

  // Initially, you can only move in a corner. Place in turn order,
  // going clockwise from top-left (0,0).
  slots_[BLUE].emplace_back(Slot{Coord(0, 0), Slot::SE});
  slots_[YELLOW].emplace_back(Slot{Coord(0, kNumCols - 1), Slot::SW});
  slots_[RED].emplace_back(Slot{Coord(kNumRows - 1, kNumCols - 1), Slot::NW});
  slots_[GREEN].emplace_back(Slot{Coord(kNumRows - 1, 0), Slot::NE});
  std::memset(slot_rows_, 0, sizeof slot_rows_);

  // Initially, everyone is allowed to move everywhere.
  for (auto color : {BLUE, YELLOW, RED, GREEN}) {
    available_[color].fill(0xFFFFFFFF);
  }

  // Track the opening moves, then mark the starting slots.
  for (auto color : {BLUE, YELLOW, RED, GREEN}) {
    AddTrackedMoves(color, 0, slot_rows_[color]);
    const Coord& c = slots_[color][0].slot.c;
    slot_rows_[color][c.row()] |= 1u << c.col();
  }
}

Board::~Board() {
}

bool Board::IsPossible(const Move& move) const {
  const TileOrientation& orientation = OrientationForMove(move);

  // Find a slot that matches one of the corners in the piece.
  // TODO(piotrf): use a map of slots to speed this up. That also solves the
  // slot duplication issue.
  for (const Corner& corner : orientation.corners()) {
    const int r = move.placement.coord.row() + corner.c.row()
        - orientation.offset().row();
    const int c = move.placement.coord.col() + corner.c.col()
        - orientation.offset().col();
    for (const SlotInfo& slot_info : slots_[move.color]) {
      if (slot_info.slot.c.row() != r || slot_info.slot.c.col() != c) continue;
      return IsPossible(slot_info.slot, orientation, corner, move.color);
    }
  }

  return false;
}

bool Board::IsPossible(const Slot& slot,
                       const TileOrientation& orientation,
                       const Corner& corner, Color color) const {
  if (!CornerFitsSlot(corner, slot)) return false;

  // Compute the coordinates of the upper-left corner.
  const int start_row = slot.c.row() - corner.c.row();
  const int start_col = slot.c.col() - corner.c.col();

  // See if the whole block fits, based on the width and height.
  if (start_row < 0 || start_row + orientation.num_rows() > kNumRows)
    return false;
  if (start_col < 0 || start_col + orientation.num_cols() > kNumCols)
    return false;

  // Now test the block against available free space on the board.
  for (int block_row = 0; block_row < orientation.num_rows(); ++block_row) {
    const int board_row = block_row + start_row;
    const uint32_t slice = (orientation.rows()[block_row]) << start_col;
    const uint32_t board_slice = available_[color][board_row];
    if ((slice | board_slice) != board_slice) {
      return false;
    }
  }

  return true;
}

// Returns a list of all possible moves for the given tile and color.
std::vector<Move> Board::PossibleMoves(const Tile& tile, Color color) const {
  std::vector<Move> moves;
  moves.reserve(200);

  // Prevent duplicate moves that can occur when different corner/slot combos
  // result in the same exact placement.
  bool placement_hashes[kNumPlacementHashes];
  std::memset(placement_hashes, 0, sizeof placement_hashes);

  Move move_template;
  move_template.tile = tile.index();
  move_template.color = color;

  for (const SlotInfo& slot_info : slots_[color]) {
    if (!slot_info.IsAvailable(tile)) continue;
    const Slot& slot = slot_info.slot;

    bool is_possible = false;
    for (const TileOrientation& orientation : tile.orientations()) {
      for (const Corner& corner : orientation.corners()) {
        if (IsPossible(slot, orientation, corner, color)) {
          is_possible = true;

          Move move = move_template;
          move.placement.coord =
              Coord(slot.c[0] + orientation.offset()[0] - corner.c[0],
                    slot.c[1] + orientation.offset()[1] - corner.c[1]);
          move.placement.rotation = orientation.rotation();
          move.placement.flip = orientation.flip();

          const int hash = PlacementHash(move.placement);
          if (placement_hashes[hash] == false) {
            placement_hashes[hash] = true;
            moves.push_back(std::move(move));
          }
        }
      }
    }

    if (!is_possible) {
      slot_info.SetUnavailable(tile);
    }
  }

  return moves;
}

bool Board::MakeMove(const Move& move) {
  if (!IsPossible(move)) {
    return false;
  }
  std::vector<Coord> coords = PlacedTile(kTiles[move.tile], move.placement);
  for (const Coord& coord : coords) {
    pieces_[coord.row()][coord.col()] = move.color;
  }

  // Update slots based on the move.
  const TileOrientation& orientation = OrientationForMove(move);
  const int first_new_slot = slots_[move.color].size();
  uint32_t old_slot_rows[kNumRows];
  std::memcpy(old_slot_rows, slot_rows_[move.color], sizeof old_slot_rows);
  for (Slot slot : orientation.slots()) {
    slot.c[0] += move.placement.coord[0] - orientation.offset()[0];
    slot.c[1] += move.placement.coord[1] - orientation.offset()[1];
    if (slot.c.row() < 0 || slot.c.row() >= kNumRows ||
        slot.c.col() < 0 || slot.c.col() >= kNumCols) {
      continue;
    }
    const uint32_t bit = 1u << slot.c.col();
    if (!(slot_rows_[move.color][slot.c.row()] & bit)) {
      slots_[move.color].emplace_back(slot);
      slot_rows_[move.color][slot.c.row()] |= bit;
    }
  }

  // Update available bitmap based on the move.
  // First, update the non-move colors. Only the blocks in the tile
  // are marked unavailable.
  const int start_row =
      move.placement.coord.row() - orientation.offset().row();
  const int start_col =
      move.placement.coord.col() - orientation.offset().col();
  for (auto color : {BLUE, YELLOW, RED, GREEN}) {
    if (color == move.color) continue;
    for (int block_row = 0; block_row < orientation.num_rows(); ++block_row) {
      const int board_row = start_row + block_row;
      const uint32_t slice = (orientation.rows()[block_row]) << start_col;
      available_[color][board_row] &= ~slice;
    }
  }
  // Next, update the move color. The blocks in the tile and all surrounding
  // blocks are marked unavailable.
  for (int block_row = 0; block_row < orientation.num_rows() + 2;
       ++block_row) {
    const int board_row = start_row + block_row - 1;
    if (board_row < 0 || board_row >= kNumRows) continue;
    uint32_t slice;
    if (start_col == 0) {
      slice = (orientation.expanded_rows()[block_row]) >> 1;
    } else {
      slice = (orientation.expanded_rows()[block_row]) << (start_col - 1);
    }
    available_[move.color][board_row] &= ~slice;
  }

  // Update the tracked moves. Other colors lose the moves overlapping the
  // tile. The move color loses all moves with this tile and the moves
  // overlapping or next to it, and gains moves through its new slots.
  const int end_row = start_row + orientation.num_rows() - 1;
  const int end_col = start_col + orientation.num_cols() - 1;
  for (auto color : {BLUE, YELLOW, RED, GREEN}) {
    if (color == move.color) continue;
    RemoveTrackedMoves(color, start_row, end_row, start_col, end_col,
                       /*placed_tile=*/-1);
  }
  placed_tiles_[move.color] |= 1u << move.tile;
  RemoveTrackedMoves(move.color, start_row - 1, end_row + 1, start_col - 1,
                     end_col + 1, move.tile);
  AddTrackedMoves(move.color, first_new_slot, old_slot_rows);

  return true;
}

std::vector<Move> Board::AllPossibleMoves(Color color) const {
  // Group the moves by tile, keeping their order within each tile.
  int tile_start[kNumTiles + 1] = {};
  for (const TrackedMove& tracked : tracked_moves_[color]) {
    ++tile_start[tracked.tile + 1];
  }
  for (int tile = 0; tile < kNumTiles; ++tile) {
    tile_start[tile + 1] += tile_start[tile];
  }

  std::vector<Move> moves(tracked_moves_[color].size());
  for (const TrackedMove& tracked : tracked_moves_[color]) {
    moves[tile_start[tracked.tile]++] = ToMove(color, tracked);
  }
  return moves;
}

Move Board::PossibleMove(Color color, int index) const {
  CHECK_GE(index, 0);
  CHECK_LT(index, NumPossibleMoves(color));
  // Find the tile of the move at `index`, then its position among the moves
  // with that tile.
  const std::array<int, kNumTiles> tile_count = NumPossibleMovesByTile(color);
  int tile = 0;
  while (index >= tile_count[tile]) {
    index -= tile_count[tile];
    ++tile;
  }
  return PossibleMoveWithTile(color, tile, index);
}

std::array<int, kNumTiles> Board::NumPossibleMovesByTile(Color color) const {
  std::array<int, kNumTiles> tile_count = {};
  for (const TrackedMove& tracked : tracked_moves_[color]) {
    ++tile_count[tracked.tile];
  }
  return tile_count;
}

Move Board::PossibleMoveWithTile(Color color, int tile, int index) const {
  CHECK_GE(index, 0);
  for (const TrackedMove& tracked : tracked_moves_[color]) {
    if (tracked.tile == tile && index-- == 0) return ToMove(color, tracked);
  }
  LOG(FATAL) << "No possible move with tile " << tile << " at index " << index;
}

Move Board::ToMove(Color color, const TrackedMove& tracked) {
  const TileOrientation& orientation =
      kTiles[tracked.tile].orientations()[tracked.orientation];
  Move move;
  move.color = color;
  move.tile = tracked.tile;
  move.placement.coord = Coord(tracked.row + orientation.offset().row(),
                               tracked.col + orientation.offset().col());
  move.placement.rotation = orientation.rotation();
  move.placement.flip = orientation.flip();
  return move;
}

void Board::AddTrackedMoves(Color color, int first_new_slot,
                            const uint32_t old_slot_rows[]) {
  // A move through a new slot that also covers an older slot was already
  // possible before, so it's already tracked. `seen_slot_rows` holds the
  // older slots, plus the new slots we've already gone through.
  uint32_t seen_slot_rows[kNumRows];
  std::memcpy(seen_slot_rows, old_slot_rows, sizeof seen_slot_rows);

  std::vector<TrackedMove>& tracked_moves = tracked_moves_[color];
  for (int i = first_new_slot; i < static_cast<int>(slots_[color].size());
       ++i) {
    const Slot& slot = slots_[color][i].slot;
    // No move can cover a slot that is already unavailable.
    if (!(available_[color][slot.c.row()] & (1u << slot.c.col()))) continue;
    // A placement is possible if all its squares are available. The window
    // has no squares off the board, so that also checks the bounds.
    const uint64_t available = WindowAround(slot, available_[color].data());
    const uint64_t seen = WindowAround(slot, seen_slot_rows);
    for (int tile = 0; tile < kNumTiles; ++tile) {
      if (placed_tiles_[color] & (1u << tile)) continue;
      for (const Tile::CornerPlacement& candidate :
           kTiles[tile].CornerPlacements(slot.type)) {
        if ((candidate.mask & ~available) || (candidate.mask & seen)) continue;
        tracked_moves.push_back(MakeTrackedMove(tile, candidate, slot.c));
      }
    }
    seen_slot_rows[slot.c.row()] |= 1u << slot.c.col();
  }
}

Board::TrackedMove Board::MakeTrackedMove(
    int tile, const Tile::CornerPlacement& candidate, const Coord& slot) {
  const TileOrientation& orientation =
      kTiles[tile].orientations()[candidate.orientation];
  const int row =
      slot.row() + candidate.placement.coord.row() - orientation.offset().row();
  const int col =
      slot.col() + candidate.placement.coord.col() - orientation.offset().col();
  return TrackedMove{orientation.packed_rows(),
                     static_cast<uint8_t>(tile),
                     static_cast<uint8_t>(candidate.orientation),
                     static_cast<int8_t>(row),
                     static_cast<int8_t>(col),
                     static_cast<int8_t>(row + orientation.num_rows() - 1),
                     static_cast<int8_t>(col + orientation.num_cols() - 1)};
}

void Board::RemoveTrackedMoves(Color color, int min_row, int max_row,
                               int min_col, int max_col, int placed_tile) {
  const std::array<uint32_t, kNumRows>& available = available_[color];
  auto is_gone = [&](const TrackedMove& tracked) {
    if (tracked.tile == placed_tile) return true;
    // Moves outside the changed area are unaffected.
    if (tracked.row > max_row || tracked.last_row < min_row ||
        tracked.col > max_col || tracked.last_col < min_col) {
      return false;
    }
    uint32_t cells = tracked.cells;
    for (int row = tracked.row; row <= tracked.last_row; ++row) {
      const uint32_t slice = (cells & 0x1f) << tracked.col;
      if ((slice & available[row]) != slice) return true;
      cells >>= 5;
    }
    return false;
  };
  std::vector<TrackedMove>& tracked_moves = tracked_moves_[color];
  tracked_moves.erase(
      std::remove_if(tracked_moves.begin(), tracked_moves.end(), is_gone),
      tracked_moves.end());
}

void Board::Print(bool debug) const {
  // Print out the pieces on the board.
  printf("  ");
  for (int c = 0; c < kNumCols; ++c) {
    printf(" %2d", c);
  }
  printf("\n");
  for (int r = 0; r < kNumRows; ++r) {
    printf("%2d ", r);
    for (int c = 0; c < kNumCols; ++c) {
      if (pieces_[r][c]) {
        std::string out = AnsiColor(pieces_[r][c]) + "\u25a3";
        printf(" %s" ANSI_COLOR_RESET " ", out.c_str());
      } else {
        printf(" _ ");
      }
    }
    printf("\n");
  }
}

}  // namespace blokus
