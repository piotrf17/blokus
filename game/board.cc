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

}  // namespace

Color NextColor(Color c) {
  switch(c) {
    case BLUE: return YELLOW;
    case YELLOW: return RED;
    case RED: return GREEN;
    case GREEN: return BLUE;
    default:
      LOG(FATAL) << "Unknown color: " << c;
  }
}

std::string ColorToString(Color c) {
  switch(c) {
    case BLUE: return "blue";
    case YELLOW: return "yellow";
    case RED: return "red";
    case GREEN: return "green";
    default:
      LOG(FATAL) << "Unknown color: " << c;
  }
}

std::string AnsiColor(Color c) {
  switch(c) {
    case BLUE: return "\x1b[34m";
    case YELLOW: return "\x1b[33m";
    case RED: return "\x1b[31m";
    case GREEN: return "\x1b[32m";
    default:
      LOG(FATAL) << "Unknown color: " << c;
  }
}

std::string Placement::DebugString() const {
  return absl::StrFormat("(%2d,%2d) %d %d", coord[0], coord[1], rotation, flip);
}

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
  memset(pieces_, 0, kWidth * kHeight * sizeof(uint8_t));

  // Initially, you can only move in a corner. Place in turn order,
  // going clockwise from top-left (0,0).
  memset(frontier_, 0, kWidth * kHeight * sizeof(uint8_t));
  frontier_[0][0] = BLUE;
  frontier_[0][kWidth - 1] = YELLOW;
  frontier_[kHeight - 1][kWidth - 1] = RED;
  frontier_[kHeight - 1][0] = GREEN;
  slots_[BLUE].push_back(Slot{Coord(0, 0), Slot::SE});
  slots_[YELLOW].push_back(Slot{Coord(0, kWidth - 1), Slot::SW});
  slots_[RED].push_back(Slot{Coord(kHeight - 1, kWidth - 1), Slot::NW});
  slots_[GREEN].push_back(Slot{Coord(kHeight - 1, 0), Slot::NE});

  // Initially, everyone is allowed to move everywhere.
  memset(allowed_, BLUE | YELLOW | RED | GREEN,
         kWidth * kHeight * sizeof(uint8_t));
}

Board::~Board() {
}

bool Board::IsPossible(const Move& move) const {
  // TODO(piotrf): fix to use slots approach
  bool contains_frontier = false;
  for (const Coord& coord : PlacedTile(kTiles[move.tile], move.placement)) {
    int x = coord[0];
    int y = coord[1];

    // Verify that the piece is on the board.
    if (x < 0 || x > kWidth - 1) return false;
    if (y < 0 || y > kHeight - 1) return false;

    // Verify that we are allowed to move here.
    if ((allowed_[x][y] & move.color) == 0) return false;

    // Keep track of whether or not we touch the frontier. We must touch in at
    // least on place.
    contains_frontier |= frontier_[coord[0]][coord[1]] & move.color;
  }
  
  return contains_frontier;
}

bool Board::IsPossible(const Slot& slot,
                       const TileOrientation& orientation,
                       const Corner& corner, Color color) const {
  if (!corner.Fits(slot)) return false;

  for (const Coord& coord : orientation.coords()) {
    const int x = coord[0] + slot.c[0] - corner.c[0];
    const int y = coord[1] + slot.c[1] - corner.c[1];

    // Verify that the piece is on the board.
    if (x < 0 || x > kWidth - 1) return false;
    if (y < 0 || y > kHeight - 1) return false;
    
    // Verify that we are allowed to move here.
    if ((allowed_[x][y] & color) == 0) return false;
  }

  return true;
}

// Returns a list of all possible moves for the given tile and color.
std::vector<Move> Board::PossibleMoves(const Tile& tile, Color color) const {
  std::vector<Move> moves;

  Move move_template;
  move_template.tile = tile.index();
  move_template.color = color;

  for (const Slot& slot : slots_.at(color)) {
    for (const TileOrientation& orientation : tile.orientations()) {
      for (const Corner& corner : orientation.corners()) {
        if (IsPossible(slot, orientation, corner, color)) {
          Move move = move_template;
          move.placement.coord =
              Coord(slot.c[0] + orientation.offset()[0] - corner.c[0],
                    slot.c[1] + orientation.offset()[1] - corner.c[1]);
          move.placement.rotation = orientation.rotation();
          move.placement.flip = orientation.flip();
          moves.push_back(std::move(move));
        }
      }
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
    pieces_[coord[0]][coord[1]] = move.color;
  }
  // TODO(piotrf): fix for slots approach
  // Update frontier and allowed based on current state of pieces_.
  for (int i = 0; i < kHeight; ++i) {
    for (int j = 0; j < kWidth; ++j) {
      // If there is a piece here, it's not on the frontier or allowed.
      if (pieces_[i][j]) {
        frontier_[i][j] = 0;
        allowed_[i][j] = 0;
        continue;
      }
      // If there is a piece horizontally or vertically separated, then
      // that color can't move here.
      uint8_t allowed = BLUE | YELLOW | RED | GREEN;
      if (i > 0 && pieces_[i - 1][j]) allowed &= ~pieces_[i - 1][j];
      if (i < kHeight - 1 && pieces_[i + 1][j]) allowed &= ~pieces_[i + 1][j];
      if (j > 0 && pieces_[i][j - 1]) allowed &= ~pieces_[i][j - 1];
      if (j < kWidth - 1 && pieces_[i][j + 1]) allowed &= ~pieces_[i][j + 1];
      allowed_[i][j] = allowed;
      // If there is a piece corner separated from us, and this is an allowed
      // spot, then this is on our frontier.
      uint8_t frontier = 0;
      if (i > 0 && j > 0 && pieces_[i - 1][j - 1])
        frontier |= pieces_[i - 1][j - 1];
      if (i > 0 && j < kWidth - 1 && pieces_[i - 1][j + 1])
        frontier |= pieces_[i - 1][j + 1];
      if (i < kHeight - 1 && j > 0 && pieces_[i + 1][j - 1])
        frontier |= pieces_[i + 1][j - 1];
      if (i < kHeight - 1 && j < kWidth - 1 && pieces_[i + 1][j + 1])
        frontier |= pieces_[i + 1][j + 1];
      frontier_[i][j] = frontier & allowed;
    }
  }
  // Always enforce initial frontier.
  // TODO(piotrf): clean up code duplication.
  frontier_[0][0] = BLUE;
  frontier_[0][kWidth - 1] = YELLOW;
  frontier_[kHeight - 1][kWidth - 1] = RED;
  frontier_[kHeight - 1][0] = GREEN;

  // Update slots based on the move.
  // TODO(piotrf): finding the matching orientation here is hacky.
  const TileOrientation* orientation = nullptr;
  for (const TileOrientation& o : kTiles[move.tile].orientations()) {
    if (o.rotation() == move.placement.rotation &&
        o.flip() == move.placement.flip) {
      orientation = &o;
      break;
    }
  }
  CHECK(orientation != nullptr);

  for (Slot slot : orientation->slots()) {
    slot.c[0] += move.placement.coord[0] - orientation->offset()[0];
    slot.c[1] += move.placement.coord[1] - orientation->offset()[1];
    // TODO(piotrf): dedup slots.
    slots_[move.color].push_back(slot);
  }

  return true;
}

void Board::PrintAllowable(const uint8_t board[kWidth][kHeight]) const {
  printf("  ");
  for (int i = 0; i < kWidth; ++i) {
    printf("  %2d ", i);
  }
  printf("\n");
  for (int j = 0; j < kHeight; ++j) {
    printf("%2d ", j);
    for (int i = 0; i < kWidth; ++i) {
      std::string out;
      for (Color color : {BLUE, YELLOW, RED, GREEN}) {
        if (board[i][j] & color)
          out += AnsiColor(color) + "\u25a3";
        else
          out +=  " ";
      }
      out += ANSI_COLOR_RESET " ";
      printf("%s", out.c_str());
    }
    printf("\n");
  }
}

void Board::Print(bool debug) const {
  // Print out the pieces on the board.
  printf("  ");
  for (int i = 0; i < kWidth; ++i) {
    printf(" %2d", i);
  }
  printf("\n");
  for (int j = 0; j < kHeight; ++j) {
    printf("%2d ", j);
    for (int i = 0; i < kWidth; ++i) {
      if (pieces_[j][i]) {
        std::string out =
            AnsiColor(static_cast<Color>(pieces_[j][i])) + "\u25a3";
        printf(" %s" ANSI_COLOR_RESET " ", out.c_str());
      } else {
        printf(" _ ");
      }
    }
    printf("\n");
  }
  if (debug) {
    printf("========== FRONTIER ==========\n");
    PrintAllowable(frontier_);
    printf("========== ALLOWED ==========\n");
    PrintAllowable(allowed_);
  }
}

}  // namespace blokus
