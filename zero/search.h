#ifndef BLOKUS_ZERO_SEARCH_H_
#define BLOKUS_ZERO_SEARCH_H_

#include <cstdint>
#include <deque>
#include <random>
#include <vector>

#include "game/board.h"
#include "game/game.h"
#include "zero/evaluator.h"

namespace blokus {

// Gumbel MCTS (Danihelka et al., "Policy improvement by planning with
// Gumbel", ICLR 2022), following the reference implementation in DeepMind's
// mctx library. Leaves are valued by the network, with no rollouts.
//
// At the root, it samples num_considered moves without replacement by
// Gumbel top-k from the policy, and spreads the simulations over them by
// sequential halving. Inside the tree, it picks the move whose visit share
// lags its improved policy the most. The improved policy is the softmax of
// the policy logits plus sigma(completed Q), where unvisited moves get the
// node's mixed value as their Q.

struct SearchOptions {
  // Network evaluations per search, not counting the root's. Simulations
  // that end in a finished game don't need one, but count.
  int num_simulations = 32;
  // Root moves to consider (k in the paper).
  int num_considered = 16;
  // Scale of the Gumbel noise. 0 considers the moves with the largest
  // priors and plays deterministically.
  double gumbel_scale = 1.0;
  // sigma(q) = (c_visit + max visits of a child) * c_scale * q, for q
  // rescaled to [0, 1] among a node's moves. mctx's defaults.
  double c_visit = 50;
  double c_scale = 0.1;
};

struct SearchResult {
  // The move to play: a pass if the root color has no moves.
  Move move;
  // The root color's possible moves, and the improved policy over them (the
  // training target).
  std::vector<Move> moves;
  std::vector<float> policy;
  // Each root move's visits.
  std::vector<int> visits;
  // The root's mixed value: the mover's player's expected score, 1 for a
  // win and 0.5 for a draw.
  float value = 0;
  int num_simulations = 0;
};

// One search, driven by its caller so that evaluations can be batched
// across searches:
//
//   GumbelSearch search(game, options, seed);
//   while (const Game* leaf = search.NextLeaf()) {
//     search.Expand(Evaluate(*leaf));
//   }
//   SearchResult result = search.Result();
//
// Only 2-player games are supported.
class GumbelSearch {
 public:
  // `game` must not be finished. It's copied.
  GumbelSearch(const Game& game, const SearchOptions& options, uint64_t seed);

  // Returns the next position for the network, which stays valid until
  // Expand, or nullptr once the search is done. The first leaf is the root.
  // Each leaf must be followed by Expand with its evaluation.
  const Game* NextLeaf();

  // Gives the network's evaluation of the last leaf.
  void Expand(const Evaluation& evaluation);

  // Returns the result. Call once NextLeaf has returned nullptr.
  SearchResult Result() const;

 private:
  struct Node {
    Game game;
    // The mover's player. Unset for finished games.
    int player = -1;
    bool finished = false;
    bool expanded = false;
    // Player 0's expected score: the network's, or the game's result.
    double value = 0;
    std::vector<Move> moves;
    std::vector<double> priors;
    std::vector<double> logits;
    // Per move: the child node (-1 until visited), its visits, and the sum
    // of player 0's values backed up through it.
    std::vector<int> children;
    std::vector<int> visits;
    std::vector<double> value_sums;
    int total_visits = 0;
  };

  // Adds a node for `game`, after playing any passes it's forced into.
  int AddNode(Game game);
  // `value` (player 0's) seen from `node`'s mover.
  double MoverValue(const Node& node, double value) const;
  // The mixed value estimate of `node`, for its mover.
  double MixedValue(const Node& node) const;
  // Completed Q values of `node`'s moves for its mover, rescaled to [0, 1].
  std::vector<double> CompletedQ(const Node& node) const;
  // sigma(completed Q) + logits, whose softmax is the improved policy.
  std::vector<double> ImprovedLogits(const Node& node) const;
  // The root move to simulate next, or -1 when the budget is spent.
  int NextRootMove();
  // The root move's score for halving and the final choice.
  double RootScore(int move, const std::vector<double>& q,
                   int max_visits) const;
  // Keeps the best half of the considered root moves.
  void Halve();
  void Backup(double value);
  int MaxVisits(const Node& node) const;

  SearchOptions options_;
  std::mt19937_64 rng_;
  std::vector<Node> nodes_;
  // The pending leaf, and the (node, move) path to it.
  int leaf_ = 0;
  std::vector<std::pair<int, int>> path_;
  int num_simulations_ = 0;
  // Root state: Gumbel noise per move, the moves still considered, and the
  // visits left in the current phase of sequential halving.
  std::vector<double> gumbels_;
  std::vector<int> considered_;
  std::deque<int> phase_;
  int num_phases_ = 0;
  bool first_phase_ = true;
};

// Runs a search with `evaluator`, one evaluation at a time.
SearchResult Search(const Game& game, const Evaluator& evaluator,
                    const SearchOptions& options, uint64_t seed);

}  // namespace blokus

#endif
