#ifndef BLOKUS_PLAYER_H_
#define BLOKUS_PLAYER_H_

#include "game/board.h"
#include "game/game.h"

namespace blokus {
  
// Interface for a blokus player. Can be a command line player, web UI player,
// AI, etc.
class Player {
 public:
  // `player_id` must be a unique number from 0...num_players.
  explicit Player(int player_id) : player_id_(player_id) {}
  
  virtual Move SelectMove(const Game& board) = 0;

  int player_id() const { return player_id_; }
  
 private:
  int player_id_;
};

}  // namespace blokus

#endif
