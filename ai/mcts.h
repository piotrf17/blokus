#ifndef BLOKUS_AI_MCTS_H
#define BLOKUS_AI_MCTS_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <string>

#include "game/player.h"

namespace blokus {

// Computes a rollout of the given game, using random actions from `rng` for
// all players' moves. Returns the id of the winning player.
int Rollout(Game game, std::mt19937_64& rng);

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

  // The number of parallel threads that are running iterations.
  int num_threads = 1;

  // With more than one thread, each node on the path an iteration selects
  // counts this many extra visits (losses) until that iteration's rollout
  // result comes back. This keeps threads from all exploring the same leaf.
  int virtual_loss = 0;

  // Seed for the AI's random number generator. If unset, seeds from
  // std::random_device. With a seed and num_threads = 1, the AI's moves are
  // reproducible. With more threads they aren't, since threads interleave
  // their updates to the shared tree.
  std::optional<uint64_t> seed;
};

struct Node;

// An AI player that uses Monte Carlo Tree Search (MCTS).
class MctsAI : public Player {
 public:
  MctsAI(int player_id, const MctsOptions& options = {});
  ~MctsAI();

  Move SelectMove(const Game& board) override;

 private:
  void Iteration(Game game, std::mt19937_64& rng);

  MctsOptions options_;
  // Only used by the thread calling SelectMove, to seed the worker threads.
  std::mt19937_64 rng_;

  std::mutex tree_mutex_;
  std::unique_ptr<Node> tree_;
};
  
}  // namespace blokus

#endif
