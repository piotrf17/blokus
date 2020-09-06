#ifndef BLOKUS_GAME_DEFS_H
#define BLOKUS_GAME_DEFS_H

#include <iomanip>
#include <string>

namespace blokus {

// Note: these values are used as indexes, so they should not be changed.
enum Color {
  INVALID = 0,
  BLUE    = 1,
  YELLOW  = 2,
  RED     = 3,
  GREEN   = 4,
};

Color NextColor(Color c);
std::string ColorToString(Color c);
std::string AnsiColor(Color c);

// Coord represents a 2-d coordinate, either local to a given tile or on the
// board. In both cases, think of this coordinate as the (row, column) in a
// matrix. (0, 0) represents the upper-left, while (0, Board::kWidth) is the
// upper-right.
struct Coord {
  Coord() {
    c[0] = 0;
    c[1] = 0;
  }
  Coord(int8_t row, int8_t col) {
    c[0] = row;
    c[1] = col;
  }

  int8_t operator[](int i) const { return c[i]; }
  int8_t& operator[](int i) { return c[i]; }

  int8_t row() const { return c[0]; }
  int8_t col() const { return c[1]; }
  
  int8_t c[2];
};

inline bool operator==(const Coord& lhs, const Coord& rhs) {
  return lhs.c[0] == rhs.c[0] && lhs.c[1] == rhs.c[1];
}

inline std::ostream& operator<<(std::ostream& os, const Coord& c) {
  os << "(" << std::setw(2) << std::setfill(' ') << int(c.row())
     << "," << std::setw(2) << std::setfill(' ') << int(c.col()) << ")";
  return os;
}

// Placement represents how to place a Tile on the board, although it does not
// include the tile itself.
struct Placement {
  Coord coord;
  int rotation = 0;  // 0..3
  bool flip = false;
  std::string DebugString() const;
};
  
}  // namespace blokus

#endif
