#include "ai/mcts.h"

#include <cmath>
#include <limits>

#include <glog/logging.h>

namespace blokus {
namespace {

Node* SelectNode(Node* node, Game* game, double c) {
  // If a leaf node, return.
  if (node->children.empty()) return node;

  // Otherwise pick the best child by UCB1 and recurse.
  std::vector<double> ucb1(
      node->children.size(), std::numeric_limits<double>::infinity());
  const double logN = std::log(node->parent->visits);
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

  CHECK(game->Move(node->move));
  return SelectNode(&node->children[max_i], board, c);
}

}  // namespace

Color MctsAI::Rollout(const Board& initial_board) {
  Game game(initial_board);
  while (!game.Finished()) {
    std::vector<Move> possible_moves = game.board().PossibleMoves();
    int move_idx = rand() % possible_moves.size();
    CHECK(game.Move(possible_moves[move_idx]));
  }
  return game.Winner();
}

void MctsAI::Iteration(const Game& initial_game) {
  Game game = initial_game;
  
  // Select a node to expand to.
  Node* node = SelectNode(&tree_, &game, c_);

  // Create all children for this node.
  std::vector<Move> possible_moves = game.PossibleMoves();
  node->children.reserve(possible_moves.size());
  for (const Move& move : possible_moves) {
    Node child_node;
    child_node.move = move;
    child_node.parent = node;
    node->children.push_back(std::move(child_node));
  }

  // Run rollouts starting from this node.
  constexpr int kNumRollouts = 1;
  for (int i = 0; i < kNumRollouts; ++i) {
    Color winner = Rollout(board);
    // xxx bookkeeping on the winner
  }
}

Move MctsAI::SelectMove(const Game& game) {
  // Unless this is our first move, update tree based on last moves.
  for (size_t i = game.moves().size() - 3; i < game.moves().size(); ++i) {
    if (tree_.visits == 0) break;
    for (Node& child : tree_.children) {
      if (child.move == game.moves()[i]) {
        tree_ = std::move(child);
        break;
      }
    }
  }

  constexpr int kNumIterations = 1;
  for (int i = 0; i < kNumIterations; ++i) {
    Iteration(game);
  }

  // Pick the best move.
  // xxx
  return {};
}

}  // namespace blokus
