#include "ai/mcts.h"

#include <atomic>
#include <cmath>
#include <limits>
#include <thread>

#include "absl/strings/str_format.h"
#include <glog/logging.h>

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

  // The number of wins tracked for having made the above move.
  int wins = 0;

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
Node* SelectNode(Node* node, Game* game, double c) {
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
        1.0 * child.wins / child.visits + c * std::sqrt(logN / child.visits);
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
  int selected_child = children_with_max[rand() % children_with_max.size()];
  
  CHECK(game->MakeMove(node->children[selected_child]->move))
      << "SelectNode tried "
      << node->children[selected_child]->move.DebugString();
  
  return SelectNode(node->children[selected_child].get(), game, c);
}


}  // namespace

std::string Node::DebugString() const {
  if (move.color == INVALID) {
    return "uninitialized";
  }
  
  const double win_rate = visits > 0 ? static_cast<float>(wins) / visits : 0.0;
  return absl::StrFormat("(%.3f %d/%d), %d children, %s", win_rate, wins,
                         visits, children.size(), move.DebugString());
}

int Rollout(Game game) {
  while (!game.Finished()) {
    std::vector<Move> possible_moves = game.PossibleMoves();
    Move move = Move::EmptyMove(game.current_color());
    if (!possible_moves.empty()) {
      move = possible_moves[rand() % possible_moves.size()];
    }
    CHECK(game.MakeMove(move));
  }
  // TODO(piotrf): use score margin as well?
  return game.Result().winner_id;
}

MctsAI::MctsAI(int player_id, const MctsOptions& options) :
    Player(player_id), options_(options), tree_(std::make_unique<Node>()) {}

MctsAI::~MctsAI() {}

void MctsAI::Iteration(Game game) {  
  // Select and possibly expand a node.
  Node* node = nullptr;
  {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    node = SelectNode(tree_.get(), &game, options_.c);
  }

  // Run rollouts on the selected node.
  for (int i = 0; i < options_.num_rollouts_per_iteration; ++i) {
    VLOG(3) << "  MCTS running rollout " << i;
    int winner = Rollout(game);
    VLOG(3) << "   rollout winner is " << winner;

    // Bookkeeping on the winner.
    std::lock_guard<std::mutex> lock(tree_mutex_);
    Node* update_node = node;
    CHECK(update_node->parent != nullptr);
    while (update_node != nullptr) {
      update_node->visits++;
      if (update_node->player == winner) {
        update_node->wins++;
      }
      update_node = update_node->parent;
    }
  }
}

Move MctsAI::SelectMove(const Game& game) {
  // Unless this is our first move, update tree based on last moves.
  for (size_t i = game.moves().size() - game.num_players() + 1;
       i < game.moves().size(); ++i) {
    if (i < 0) continue;
    VLOG(1) << "MCTS updating tree for move " << game.moves()[i].DebugString();
    VLOG(1) << " current tree_: " << tree_->DebugString();
    if (tree_->children.empty()) {
      VLOG(1) << "MCTS tree ran out while updating nodes";
      tree_ = std::make_unique<Node>();
      break;
    }
    bool found_match = false;
    for (std::unique_ptr<Node>& child : tree_->children) {
      VLOG(2) << "  child: " << child->DebugString();
      if (child->move == game.moves()[i]) {
        VLOG(2) << "    Match found, stopping.";
        found_match = true;
        tree_ = std::move(child);
        tree_->parent = nullptr;
        break;
      }
    }
    CHECK(found_match);
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
  if (tree_->children.size() == 1) {
    tree_ = std::move(tree_->children[0]);
    return tree_->move;
  }

  // Run MCTS iterations.
  {
    // Launch all worker threads.
    std::vector<std::thread> workers;
    std::atomic<int> counter(0);
    for (int i = 0; i < options_.num_threads; ++i) {
      workers.emplace_back([&]() {
        while(true) {
          if (counter.fetch_add(1) >= options_.num_iterations) return;
          Iteration(game);
        }
      });
    }
    // Join worker threads.
    for (std::thread& worker : workers) {
      worker.join();
    }
  }

  // Pick the best move.
  VLOG(1) << "MCTS picking from " << tree_->children.size() << " moves.";
  int max_visits = 0;
  std::unique_ptr<Node>* best_child = nullptr;
  for (std::unique_ptr<Node>& child : tree_->children) {
    VLOG(2) << child->DebugString();
    if (child->visits > max_visits) {
      max_visits = child->visits;
      best_child = &child;
    }
  }
  CHECK(best_child != nullptr);
  VLOG(0) << "player " << player_id() << " estimate of winning = "
          << static_cast<double>((*best_child)->wins) / (*best_child)->visits;
  tree_ = std::move(*best_child);
  return tree_->move;
}

}  // namespace blokus
