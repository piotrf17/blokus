#include "game/board.h"

#include <cstring>
#include <glog/logging.h>

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

constexpr int kMaxPlacementHash =
    RC(Board::kNumRows - 1, Board::kNumCols - 1) * 8 + 3 * 2 + 1;

}  // namespace

Move Move::EmptyMove(Color color) {
  Move move;
  move.color = color;
  move.tile = -1;
  return move;
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
  slots_.resize(5);
  slot_map_.resize(5);
  slots_[BLUE].emplace_back(Slot{Coord(0, 0), Slot::SE});
  slot_map_[BLUE] = std::vector<bool>(kNumRows * kNumCols, false);
  slot_map_[BLUE][RC(0, 0)] = true;
  slots_[YELLOW].emplace_back(Slot{Coord(0, kNumCols - 1), Slot::SW});
  slot_map_[YELLOW] = std::vector<bool>(kNumRows * kNumCols, false);
  slot_map_[YELLOW][RC(0, kNumCols -1)] = true;
  slots_[RED].emplace_back(Slot{Coord(kNumRows - 1, kNumCols - 1), Slot::NW});
  slot_map_[RED] = std::vector<bool>(kNumRows * kNumCols, false);
  slot_map_[RED][RC(kNumRows - 1, kNumCols - 1)] = true;
  slots_[GREEN].emplace_back(Slot{Coord(kNumRows - 1, 0), Slot::NE});
  slot_map_[GREEN] = std::vector<bool>(kNumRows * kNumCols, false);
  slot_map_[GREEN][RC(kNumRows - 1, 0)] = true;

  // Initially, everyone is allowed to move everywhere.
  available_.resize(5);
  for (auto color : {BLUE, YELLOW, RED, GREEN}) {
    for (int row = 0; row < kNumRows; ++row) {
      available_[color].push_back(0xFFFFFFFF);
    }
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
  bool placement_hashes[kMaxPlacementHash];
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
  for (Slot slot : orientation.slots()) {
    slot.c[0] += move.placement.coord[0] - orientation.offset()[0];
    slot.c[1] += move.placement.coord[1] - orientation.offset()[1];
    if (slot.c.row() < 0 || slot.c.row() >= kNumRows ||
        slot.c.col() < 0 || slot.c.col() >= kNumCols) {
      continue;
    }
    const int rc = RC(slot.c.row(), slot.c.col());
    if (!slot_map_[move.color][rc]) {
      slots_[move.color].emplace_back(slot);
      slot_map_[move.color][rc] = true;
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

  return true;
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
