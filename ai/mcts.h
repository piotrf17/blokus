#ifndef BLOKUS_AI_MCTS_H
#define BLOKUS_AI_MCTS_H

#include <set>

#include "game/player.h"

namespace blokus {

struct Node {
  Move move;
  int wins;
  int visits;

  Node* parent;
  std::vector<Node> children;
};

class MctsAI : public Player {
 public:
  explicit MctsAI(Color color) : Player(color) {}

  bool SelectMove(const Board& board, Move* move, int* chosen_tile) override;

 private:
  void Iteration(const Board& initial_board);
  Color Rollout(const Board& board);
  
  double c_ = 1.4;
  Node tree_;
  std::set<int> played_tiles_; 
};
  
}  // namespace blokus

#endif
