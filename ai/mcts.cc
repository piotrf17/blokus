#include "ai/mcts.h"

#include <cmath>
#include <limits>

#include "absl/strings/str_format.h"
#include <glog/logging.h>

namespace blokus {
namespace {

Node* SelectNode(Node* node, Game* game, double c) {
  // If a leaf node, return.
  if (node->children.empty()) return node;

  // Otherwise pick the best child by UCB1 and recurse.
  std::vector<double> ucb1(
      node->children.size(), std::numeric_limits<double>::infinity());
  const double logN = std::log(node->visits);
  for (size_t i = 0; i < node->children.size(); ++i) {
    const Node& child = node->children[i];
    if (child.visits == 0) continue;
    ucb1[i] =
        1.0 * child.wins / child.visits + c * std::sqrt(logN / child.visits);
  }

  double max_ucb1 = ucb1[0];
  int max_i = 0;
  for (size_t i = 1; i < node->children.size(); ++i) {
    if (ucb1[i] > max_ucb1) {
      max_ucb1 = ucb1[i];
      max_i = i;
    }
  }
  
  CHECK(game->MakeMove(node->children[max_i].move))
      << "SelectNode tried " << node->move.DebugString();
  
  return SelectNode(&node->children[max_i], game, c);
}

void ExpandNode(const Game& game, Node* node) {
  if (!node->children.empty()) return;

  // Look for possible moves, and if found, create a child for each move.
  std::vector<Move> possible_moves = game.PossibleMoves();
  node->children.reserve(possible_moves.size());
  for (const Move& move : possible_moves) {
    Node child_node;
    child_node.move = move;
    child_node.parent = node;
    node->children.push_back(std::move(child_node));
  }

  // If there are no possible moves, create an empty move for this node.
  if (node->children.empty()) {
    Node child_node;
    child_node.move = Move::EmptyMove(game.current_color());
    child_node.parent = node;
    node->children.push_back(std::move(child_node));
  }
}

}  // namespace

std::string Node::DebugString() const {
  return absl::StrFormat("(%d/%d), %d children, %s", wins, visits,
                         children.size(), move.DebugString());
}

Color Rollout(Game game) {
  while (!game.Finished()) {
    std::vector<Move> possible_moves = game.PossibleMoves();
    Move move = Move::EmptyMove(game.current_color());
    if (!possible_moves.empty()) {
      move = possible_moves[rand() % possible_moves.size()];
    }
    CHECK(game.MakeMove(move));
  }
  // TODO(piotrf): use score margin as well?
  return game.Result().winner;
}

void MctsAI::Iteration(Game game) {  
  // Select and expand a node.
  Node* node = SelectNode(&tree_, &game, c_);
  ExpandNode(game, node);

  // Arbitrarily pick the first child to rollout on.
  node = &node->children[0];
  constexpr int kNumRollouts = 1;
  for (int i = 0; i < kNumRollouts; ++i) {
    VLOG(2) << "  MCTS running rollout " << i;
    Color winner = Rollout(game);
    VLOG(2) << "    rollout winner is " << ColorToString(winner);

    // Bookkeeping on the winner.
    Node* update_node = node;
    while (update_node != nullptr) {
      update_node->visits++;
      if (update_node->move.color == winner) {
        update_node->wins++;
      }
      update_node = update_node->parent;
    }
  }
}

Move MctsAI::SelectMove(const Game& game) {
  // Unless this is our first move, update tree based on last moves.
  for (size_t i = game.moves().size() - 3; i < game.moves().size(); ++i) {
    VLOG(1) << "MCTS updating tree for move " << game.moves()[i].DebugString();
    VLOG(1) << " current tree_: " << tree_.DebugString();
    if (tree_.children.empty()) {
      VLOG(1) << "MCTS tree ran out while updating nodes";
      tree_ = Node();
      break;
    }
    bool found_match = false;
    for (Node& child : tree_.children) {
      VLOG(2) << "  child: " << child.DebugString();
      if (child.move == game.moves()[i]) {
        found_match = true;
        tree_ = std::move(child);
        break;
      }
    }
    CHECK(found_match);
  }

  // Expand out the root, in case we didn't find it above.
  ExpandNode(game, &tree_);

  constexpr int kNumIterations = 1000;
  for (int i = 0; i < kNumIterations; ++i) {
    VLOG(2) << "MCTS running iteration " << i;
    Iteration(game);
  }

  if (tree_.children.empty()) {
    return Move::EmptyMove(color());
  }

  // Pick the best move.
  VLOG(1) << "MCTS picking from " << tree_.children.size() << " moves.";
  int max_visits = 0;
  const Node* best_child = nullptr;
  for (const Node& child : tree_.children) {
    VLOG(2) << child.DebugString();
    if (child.visits > max_visits) {
      max_visits = child.visits;
      best_child = &child;
    }
  }
  CHECK(best_child != nullptr);
  tree_ = std::move(*best_child);
  return tree_.move;
}

}  // namespace blokus
