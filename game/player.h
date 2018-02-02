#ifndef BLOKUS_PLAYER_H_
#define BLOKUS_PLAYER_H_

#include "board.h"

namespace blokus {
  
// Interface for a blokus player. Can be a command line player, web UI player,
// AI, etc.
class Player {
 public:
  explicit Player(Color color) : color_(color) {}
  
  virtual bool SelectMove(const Board& board, Move* move, int* chosen_tile) = 0;

  Color color() const { return color_; }
  
 private:
  Color color_;
};

}  // namespace blokus

#endif
