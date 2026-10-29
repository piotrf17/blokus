#ifndef BLOKUS_AI_MCTS_H
#define BLOKUS_AI_MCTS_H

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <string>

#include "game/player.h"

namespace blokus {

// A reward per player id. Entries past the number of players are 0.
using PlayerRewards = std::array<double, 4>;

// Returns the rewards for a finished game: 1 for the sole top scorer, or 1/k
// for each of k players tied for the top score, and 0 for everyone else.
PlayerRewards Rewards(const GameResult& result);

// Mixes Rewards with a reward for each player's score margin over the best
// other player: (1 - margin_weight) * Rewards(result) + margin_weight *
// (0.5 + 0.5 * tanh(margin / margin_scale)). With margin_weight 0 this is
// Rewards(result).
PlayerRewards Rewards(const GameResult& result, double margin_weight,
                      double margin_scale);

// How rollouts pick moves (see RolloutMove).
struct RolloutOptions {
  // Each possible move is weighted by exp(size_bias * size of its tile), so a
  // positive value favors larger tiles. 0 weights every move equally.
  double size_bias = 1;

  // How much a rollout's reward depends on score margins rather than only
  // on who won, and the margin that counts as decisive (see Rewards).
  double margin_weight = 0;
  double margin_scale = 10;
};

// Returns a random move for the current color of `game`, picked as
// `options` describes, or a pass if it has no possible moves.
Move RolloutMove(const Game& game, const RolloutOptions& options,
                 std::mt19937_64& rng);

// Computes a rollout of the given game, playing RolloutMove for all players,
// using `rng`. Returns the Rewards of the finished game, with `options`'
// margin_weight and margin_scale.
PlayerRewards Rollout(Game game, std::mt19937_64& rng,
                      const RolloutOptions& options = {});

struct MctsOptions {
  // The exploration parameter for UCB1.
  // Setting this higher favors exploration more over exploitation. Theory
  // suggests sqrt(2), but that explores far too much here. 0.25 was the best
  // value in main:match at 10k iterations, and as good as anything lower at
  // 1000 (see analysis/mcts_tuning.md).
  double c = 0.25;

  // The number of iterations of MCTS to run per move, with each iteration
  // consisting of selection of a leaf node, expansion of that node, rollout,
  // and backpropogation of rollout results.
  int num_iterations = 10000;

  // The number of random rollouts to run per MCTS iteration.
  int num_rollouts_per_iteration = 1;

  // How rollouts pick moves and score finished games.
  RolloutOptions rollout;

  // The number of parallel threads that are running iterations.
  int num_threads = 1;

  // With more than one thread, each node on the path an iteration selects
  // counts this many extra visits (losses) until that iteration's rollout
  // result comes back. This keeps threads from all exploring the same leaf.
  // At 10k iterations with 4 threads it measured as neutral to slightly
  // helpful (see analysis/mcts_tuning.md).
  int virtual_loss = 3;

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
