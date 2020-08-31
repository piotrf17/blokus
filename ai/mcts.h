#ifndef BLOKUS_AI_MCTS_H
#define BLOKUS_AI_MCTS_H

#include <memory>
#include <set>
#include <string>

#include "game/player.h"

namespace blokus {

// Computes a rollout of the given game, using random actions for all players
// moves. Returns the id of the winning player.
int Rollout(Game game);

struct MctsOptions {
  // The exploration parameter for UCB1.
  // Setting this higher favors exploration more over exploitation.
  // Theory dictates that this should be approximately sqrt(2).
  double c = 1.4;

  // The number of iterations of MCTS to run per move, with each iteration
  // consisting of selection of a leaf node, expansion of that node, rollout,
  // and backpropogation of rollout results.
  int num_iterations = 10000;

  // The number of random rollouts to run per MCTS iteration.
  int num_rollouts_per_iteration = 1;
};

struct Node;

// An AI player that uses Monte Carlo Tree Search (MCTS).
class MctsAI : public Player {
 public:
  MctsAI(int player_id, const MctsOptions& options = {});
  ~MctsAI();

  Move SelectMove(const Game& board) override;

 private:
  void Iteration(Game game);

  MctsOptions options_;

  std::set<int> played_tiles_;

  std::unique_ptr<Node> tree_;
};
  
}  // namespace blokus

#endif
