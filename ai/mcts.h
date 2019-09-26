#ifndef BLOKUS_AI_MCTS_H
#define BLOKUS_AI_MCTS_H

#include <set>

#include "game/player.h"

namespace blokus {

class MctsAI : public Player {
 public:
  explicit MctsAI(Color color) : Player(color) {}

  bool SelectMove(const Board& board, Move* move, int* chosen_tile) override;

 private:
  std::set<int> played_tiles_; 
};
  
}  // namespace blokus

#endif
