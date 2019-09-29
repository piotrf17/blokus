#ifndef BLOKUS_AI_RANDOM_H
#define BLOKUS_AI_RANDOM_H

#include <set>

#include "game/player.h"

namespace blokus {

// An AI that just plays a random move from the set of possible moves.
class RandomAI : public Player {
 public:
  explicit RandomAI(Color color) : Player(color) {}

  Move SelectMove(const Game& game) override;

 private:
  std::set<int> played_tiles_;
};

}  // namespace blokus

#endif
