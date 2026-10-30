#include "ai/mcts.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <random>
#include <thread>
#include <tuple>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_format.h"

#include "game/tile.h"

namespace blokus {

// A node in the game tree.
// Technically, this also includes edges going out from this node.
struct Node {
  std::string DebugString() const;

  // The move that caused us to arrive at this node, i.e. the incoming edge
  // to this node.
  Move move;

  // The player that played the above move.
  int player = -1;

  // The total reward for `player` over the rollouts through this node: 1 per
  // win, and a share of 1 per tie (see Rewards).
  double wins = 0;

  // The number of times rollouts have visited the above move.
  int visits = 0;

  Node* parent = nullptr;
  std::vector<std::unique_ptr<Node>> children;
};

namespace {

bool ShouldExpand(const Game& game, const Node& node) {
  if (game.Finished()) return false;
  CHECK(node.parent != nullptr);
  CHECK(!node.parent->children.empty());
  return node.parent->visits >= node.parent->children.size();
}

void ExpandNode(const Game& game, Node* node) {
  CHECK(node->children.empty()) << "Expanding a non-leaf node: "
                                << node->DebugString();

  // Look for possible moves, and if found, create a child for each move.
  std::vector<Move> possible_moves = game.PossibleMoves();
  node->children.reserve(possible_moves.size());
  for (const Move& move : possible_moves) {
    auto child_node = std::make_unique<Node>();
    child_node->move = move;
    child_node->player = game.current_player();
    child_node->parent = node;
    node->children.push_back(std::move(child_node));
  }

  // If there are no possible moves, create an empty move for this node.
  if (node->children.empty()) {
    auto child_node = std::make_unique<Node>();
    child_node->move = Move::EmptyMove(game.current_color());
    child_node->player = game.current_player();
    child_node->parent = node;
    node->children.push_back(std::move(child_node));
  }
}

// Returns a pointer to a leaf-node in the game tree starting from `node`.
// The `game` is modified to reflect the state as moves are made following
// the nodes recursiverly down.
//
// A leaf node is defined as a node that has no children.
//
// Note that this function also includes the "expansion" phase in usual
// MCTS terminology. A leaf node is only expanded if all siblings have been
// visited at least once. After expansion, we return a child.
//
// Adds `virtual_loss` visits, without wins, to every node on the path, so
// that other threads selecting before this iteration's backprop are steered
// elsewhere. Backprop takes them back out.
Node* SelectNode(Node* node, Game* game, double c, int virtual_loss,
                 std::mt19937_64& rng) {
  node->visits += virtual_loss;

  // If a leaf node, possibly expand it and continue selection.
  if (node->children.empty()) {
    if (!ShouldExpand(*game, *node)) {
      return node;
    }
    ExpandNode(*game, node);
  }

  // Pick the best child by UCB1 and recurse.
  std::vector<double> ucb1(
      node->children.size(), std::numeric_limits<double>::infinity());
  const double logN = std::log(node->visits);
  for (size_t i = 0; i < node->children.size(); ++i) {
    const Node& child = *node->children[i];
    if (child.visits == 0) continue;
    ucb1[i] =
        child.wins / child.visits + c * std::sqrt(logN / child.visits);
  }

  // If there are multiple children with the max UCB1, then select randomly.
  // This prevents biasing towards certain moves at the start of expansion.
  double max_ucb1 = *std::max_element(ucb1.begin(), ucb1.end());
  std::vector<int> children_with_max;
  for (size_t i = 0; i < node->children.size(); ++i) {
    if (ucb1[i] == max_ucb1) {
      children_with_max.push_back(i);
    }
  }
  std::uniform_int_distribution<int> pick(0, children_with_max.size() - 1);
  const int selected_child = children_with_max[pick(rng)];
  
  CHECK(game->MakeMove(node->children[selected_child]->move))
      << "SelectNode tried "
      << node->children[selected_child]->move.DebugString();
  
  return SelectNode(node->children[selected_child].get(), game, c,
                    virtual_loss, rng);
}


}  // namespace

std::string Node::DebugString() const {
  if (move.color == INVALID) {
    return "uninitialized";
  }
  
  const double win_rate = visits > 0 ? wins / visits : 0.0;
  return absl::StrFormat("(%.3f %.1f/%d), %d children, %s", win_rate, wins,
                         visits, children.size(), move.DebugString());
}

PlayerRewards Rewards(const GameResult& result) {
  const int max_score =
      *std::max_element(result.scores.begin(), result.scores.end());
  const int num_winners =
      std::count(result.scores.begin(), result.scores.end(), max_score);
  PlayerRewards rewards = {};
  for (int i = 0; i < static_cast<int>(result.scores.size()); ++i) {
    if (result.scores[i] == max_score) rewards[i] = 1.0 / num_winners;
  }
  return rewards;
}

PlayerRewards Rewards(const GameResult& result, double margin_weight,
                      double margin_scale) {
  PlayerRewards rewards = Rewards(result);
  if (margin_weight == 0) return rewards;
  const int num_players = result.scores.size();
  for (int i = 0; i < num_players; ++i) {
    int best_other = std::numeric_limits<int>::min();
    for (int j = 0; j < num_players; ++j) {
      if (j != i) best_other = std::max(best_other, result.scores[j]);
    }
    const double margin = result.scores[i] - best_other;
    const double margin_reward = 0.5 + 0.5 * std::tanh(margin / margin_scale);
    rewards[i] =
        (1 - margin_weight) * rewards[i] + margin_weight * margin_reward;
  }
  return rewards;
}

namespace {

// Picks rollout moves as RolloutOptions describes, with the tile weights
// computed once.
class RolloutPolicy {
 public:
  explicit RolloutPolicy(const RolloutOptions& options)
      : uniform_(options.size_bias == 0) {
    for (int tile = 0; tile < kNumTiles; ++tile) {
      weights_[tile] = std::exp(options.size_bias * kTiles[tile].Size());
    }
  }

  Move PickMove(const Game& game, std::mt19937_64& rng) const {
    const int num_moves = game.NumPossibleMoves();
    if (num_moves == 0) return Move::EmptyMove(game.current_color());
    if (uniform_) {
      std::uniform_int_distribution<int> pick(0, num_moves - 1);
      return game.PossibleMove(pick(rng));
    }

    // Picks a tile with probability proportional to its number of moves times
    // its weight, then one of its moves uniformly.
    const std::array<int, kNumTiles> counts = game.NumPossibleMovesByTile();
    double total = 0;
    int last_tile = -1;
    for (int tile = 0; tile < kNumTiles; ++tile) {
      if (counts[tile] == 0) continue;
      total += counts[tile] * weights_[tile];
      last_tile = tile;
    }
    CHECK_GE(last_tile, 0);
    double r = std::uniform_real_distribution<double>(0, total)(rng);
    // Rounding can leave r just past the last tile, so that's the fallback.
    int tile = last_tile;
    for (int t = 0; t < last_tile; ++t) {
      r -= counts[t] * weights_[t];
      if (r < 0) {
        tile = t;
        break;
      }
    }
    std::uniform_int_distribution<int> pick(0, counts[tile] - 1);
    return game.PossibleMoveWithTile(tile, pick(rng));
  }

 private:
  const bool uniform_;
  std::array<double, kNumTiles> weights_;
};

}  // namespace

Move RolloutMove(const Game& game, const RolloutOptions& options,
                 std::mt19937_64& rng) {
  return RolloutPolicy(options).PickMove(game, rng);
}

PlayerRewards Rollout(Game game, std::mt19937_64& rng,
                      const RolloutOptions& options) {
  const RolloutPolicy policy(options);
  while (!game.Finished()) {
    CHECK(game.MakeMove(policy.PickMove(game, rng)));
  }
  return Rewards(game.Result(), options.margin_weight, options.margin_scale);
}

MctsAI::MctsAI(int player_id, const MctsOptions& options)
    : Player(player_id),
      options_(options),
      rng_(options.seed.has_value() ? *options.seed : std::random_device()()),
      tree_(std::make_unique<Node>()) {}

MctsAI::~MctsAI() {}

void MctsAI::Iteration(Game game, std::mt19937_64& rng) {
  // Select and possibly expand a node. Virtual loss only matters with
  // concurrent iterations, so a single thread doesn't use it.
  const int virtual_loss = options_.num_threads > 1 ? options_.virtual_loss : 0;
  Node* node = nullptr;
  {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    node = SelectNode(tree_.get(), &game, options_.c, virtual_loss, rng);
  }

  // Run rollouts on the selected node.
  for (int i = 0; i < options_.num_rollouts_per_iteration; ++i) {
    // TODO(piotrf): re-enable vlog once absl supports it
    //  VLOG(3) << "  MCTS running rollout " << i;
    const PlayerRewards rewards = Rollout(game, rng, options_.rollout);

    // Credit each node with the reward of the player who made its move.
    std::lock_guard<std::mutex> lock(tree_mutex_);
    Node* update_node = node;
    CHECK(update_node->parent != nullptr);
    while (update_node != nullptr) {
      // The first rollout's visit replaces the virtual loss from selection.
      update_node->visits += i == 0 ? 1 - virtual_loss : 1;
      // The root has no move, and its player is -1.
      if (update_node->player >= 0) {
        update_node->wins += rewards[update_node->player];
      }
      update_node = update_node->parent;
    }
  }
}

Move MctsAI::SelectMove(const Game& game) {
  // Unless this is our first move, follow the other players' moves made since
  // our last move down the tree. If any of them aren't in the tree (it was
  // never expanded that far, or a color passed early), start a new tree.
  const int num_moves = game.moves().size();
  for (int i = std::max(0, num_moves - game.num_players() + 1); i < num_moves;
       ++i) {
    // TODO(piotrf): re-enable vlog once absl supports it
    //  VLOG(1) << "MCTS updating tree for move " << game.moves()[i].DebugString();
    //  VLOG(1) << " current tree_: " << tree_->DebugString();
    std::unique_ptr<Node>* match = nullptr;
    for (std::unique_ptr<Node>& child : tree_->children) {
      if (child->move == game.moves()[i]) {
        match = &child;
        break;
      }
    }
    if (match == nullptr) {
      // TODO(piotrf): re-enable vlog once absl supports it
      //  VLOG(1) << "MCTS tree has no node for " << game.moves()[i].DebugString();
      tree_ = std::make_unique<Node>();
      break;
    }
    tree_ = std::move(*match);
    tree_->parent = nullptr;
  }

  // Expand out the root, in case we didn't find it above.
  if (tree_->children.empty()) {
    ExpandNode(game, tree_.get());
  }
  CHECK_GT(tree_->children.size(), 0);

  // If there is only a single move available, take it. In theory, we could
  // spend some time planning for future moves, but:
  //   1) we're not playing in a timed environment.
  //   2) it's rare that a single move will lead to many future moves.
  root_stats_.clear();
  if (tree_->children.size() == 1) {
    root_stats_.push_back({.move = tree_->children[0]->move});
    tree_ = std::move(tree_->children[0]);
    tree_->parent = nullptr;
    return tree_->move;
  }

  // Run MCTS iterations.
  {
    // Launch all worker threads, each with its own random number generator.
    std::vector<std::thread> workers;
    std::atomic<int> counter(0);
    for (int i = 0; i < options_.num_threads; ++i) {
      workers.emplace_back([&, seed = rng_()]() {
        std::mt19937_64 rng(seed);
        while (true) {
          if (counter.fetch_add(1) >= options_.num_iterations) return;
          Iteration(game, rng);
        }
      });
    }
    // Join worker threads.
    for (std::thread& worker : workers) {
      worker.join();
    }
  }

  for (const std::unique_ptr<Node>& child : tree_->children) {
    root_stats_.push_back(
        {.move = child->move, .visits = child->visits, .wins = child->wins});
  }

  // Early in the game, sample a move in proportion to visits.
  if (num_moves < options_.num_sampled_moves) {
    std::vector<int> visits;
    for (const MoveStats& stats : root_stats_) visits.push_back(stats.visits);
    std::discrete_distribution<int> sample(visits.begin(), visits.end());
    tree_ = std::move(tree_->children[sample(rng_)]);
    tree_->parent = nullptr;
    return tree_->move;
  }

  // Pick the best move.
  // TODO(piotrf): re-enable vlog once absl supports it
  //  VLOG(1) << "MCTS picking from " << tree_->children.size() << " moves.";
  // The most-visited child, breaking ties by total reward and then randomly.
  // With fewer iterations than moves, many children tie at one visit, and
  // children are in tile order, so taking the first tie would favor the
  // smallest tiles.
  std::vector<std::unique_ptr<Node>*> best_children;
  for (std::unique_ptr<Node>& child : tree_->children) {
    // TODO(piotrf): re-enable vlog once absl supports it
    //  VLOG(2) << child->DebugString();
    if (!best_children.empty()) {
      const Node& best = **best_children.front();
      const auto key = std::tie(child->visits, child->wins);
      const auto best_key = std::tie(best.visits, best.wins);
      if (key < best_key) continue;
      if (key > best_key) best_children.clear();
    }
    best_children.push_back(&child);
  }
  CHECK(!best_children.empty());
  std::uniform_int_distribution<int> pick(0, best_children.size() - 1);
  std::unique_ptr<Node>* best_child = best_children[pick(rng_)];
  // TODO(piotrf): re-enable vlog once absl supports it
  //  VLOG(0) << "player " << player_id() << " estimate of winning = "
  //          << (*best_child)->wins / (*best_child)->visits;
  tree_ = std::move(*best_child);
  tree_->parent = nullptr;
  return tree_->move;
}

}  // namespace blokus
