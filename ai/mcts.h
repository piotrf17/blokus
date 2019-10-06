#ifndef BLOKUS_AI_MCTS_H
#define BLOKUS_AI_MCTS_H

#include <set>
#include <string>

#include "game/player.h"

namespace blokus {

// A node in the game tree.
// Technically, this also includes edges going out from this node.
struct Node {
  std::string DebugString() const;

  // The move that caused us to arrive at this node.
  Move move;

  // The number of wins tracked for having made the above move.
  int wins = 0;

  // The number of times rollouts have visited the above move.
  int visits = 0;

  Node* parent = nullptr;
  std::vector<Node> children;
};

class MctsAI : public Player {
 public:
  explicit MctsAI(Color color) : Player(color) {}

  Move SelectMove(const Game& board) override;

 private:
  void Iteration(Game game);
  Color Rollout(Game game);

  double c_ = 1.4;

  std::set<int> played_tiles_;

  Node tree_;
};
  
}  // namespace blokus

#endif
