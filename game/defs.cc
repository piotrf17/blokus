#include "game/defs.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"

namespace blokus {

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

}
