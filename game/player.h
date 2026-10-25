#ifndef BLOKUS_GAME_PLAYER_H_
#define BLOKUS_GAME_PLAYER_H_

#include "game/board.h"
#include "game/game.h"

namespace blokus {

// Interface for a blokus player. Can be a command line player, web UI player,
// AI, etc.
class Player {
 public:
  // `player_id` must be a unique number from 0 to num_players - 1.
  explicit Player(int player_id) : player_id_(player_id) {}
  virtual ~Player() = default;

  // Returns the move to play for `game.current_color()`. The move must be
  // legal, or a pass.
  virtual Move SelectMove(const Game& game) = 0;

  int player_id() const { return player_id_; }

 private:
  int player_id_;
};

}  // namespace blokus

#endif  // BLOKUS_GAME_PLAYER_H_
