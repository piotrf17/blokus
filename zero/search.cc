#include "zero/search.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <utility>

#include "absl/log/check.h"

namespace blokus {
namespace {

// The expected score for player 0 of a finished game.
double ResultValue(const Game& game) {
  const GameResult result = game.Result();
  if (result.scores[0] == result.scores[1]) return 0.5;
  return result.scores[0] > result.scores[1] ? 1.0 : 0.0;
}

std::vector<double> Softmax(const std::vector<double>& logits) {
  const double max = *std::max_element(logits.begin(), logits.end());
  std::vector<double> probs(logits.size());
  double sum = 0;
  for (size_t i = 0; i < logits.size(); ++i) {
    probs[i] = std::exp(logits[i] - max);
    sum += probs[i];
  }
  for (double& p : probs) p /= sum;
  return probs;
}

}  // namespace

GumbelSearch::GumbelSearch(const Game& game, const SearchOptions& options,
                           uint64_t seed)
    : options_(options), rng_(seed) {
  CHECK_EQ(game.num_players(), 2) << "Only 2-player games are supported";
  CHECK(!game.Finished());
  CHECK_GE(options.num_considered, 1);
  // The root is never forced to pass: with no moves, the search is empty.
  Node root{.game = game, .player = game.current_player()};
  nodes_.push_back(std::move(root));
}

int GumbelSearch::AddNode(Game game) {
  while (!game.Finished() && game.NumPossibleMoves() == 0) {
    CHECK(game.MakeMove(Move::EmptyMove(game.current_color())));
  }
  Node node{.game = std::move(game)};
  if (node.game.Finished()) {
    node.finished = true;
    node.value = ResultValue(node.game);
  } else {
    node.player = node.game.current_player();
  }
  nodes_.push_back(std::move(node));
  return nodes_.size() - 1;
}

double GumbelSearch::MoverValue(const Node& node, double value) const {
  return node.player == 0 ? value : 1 - value;
}

double GumbelSearch::MixedValue(const Node& node) const {
  const double value = MoverValue(node, node.value);
  if (node.total_visits == 0) return value;
  double visited_prior = 0;
  double weighted_q = 0;
  for (size_t i = 0; i < node.moves.size(); ++i) {
    if (node.visits[i] == 0) continue;
    visited_prior += node.priors[i];
    weighted_q += node.priors[i] *
                  MoverValue(node, node.value_sums[i] / node.visits[i]);
  }
  // Priors can underflow to 0; then the visited Q values count equally.
  if (visited_prior <= 0) {
    double sum = 0;
    int count = 0;
    for (size_t i = 0; i < node.moves.size(); ++i) {
      if (node.visits[i] == 0) continue;
      sum += MoverValue(node, node.value_sums[i] / node.visits[i]);
      ++count;
    }
    return (value + node.total_visits * sum / count) /
           (1 + node.total_visits);
  }
  return (value + node.total_visits * weighted_q / visited_prior) /
         (1 + node.total_visits);
}

std::vector<double> GumbelSearch::CompletedQ(const Node& node) const {
  const double mixed = MixedValue(node);
  std::vector<double> q(node.moves.size());
  for (size_t i = 0; i < q.size(); ++i) {
    q[i] = node.visits[i] > 0
               ? MoverValue(node, node.value_sums[i] / node.visits[i])
               : mixed;
  }
  const auto [min, max] = std::minmax_element(q.begin(), q.end());
  const double low = *min;
  const double range = std::max(*max - *min, 1e-8);
  for (double& value : q) value = (value - low) / range;
  return q;
}

int GumbelSearch::MaxVisits(const Node& node) const {
  return node.visits.empty()
             ? 0
             : *std::max_element(node.visits.begin(), node.visits.end());
}

std::vector<double> GumbelSearch::ImprovedLogits(const Node& node) const {
  const std::vector<double> q = CompletedQ(node);
  const double sigma_scale =
      (options_.c_visit + MaxVisits(node)) * options_.c_scale;
  std::vector<double> logits(node.moves.size());
  for (size_t i = 0; i < logits.size(); ++i) {
    logits[i] = node.logits[i] + sigma_scale * q[i];
  }
  return logits;
}

double GumbelSearch::RootScore(int move, const std::vector<double>& q,
                               int max_visits) const {
  const Node& root = nodes_[0];
  return gumbels_[move] + root.logits[move] +
         (options_.c_visit + max_visits) * options_.c_scale * q[move];
}

void GumbelSearch::Halve() {
  const Node& root = nodes_[0];
  const std::vector<double> q = CompletedQ(root);
  const int max_visits = MaxVisits(root);
  std::stable_sort(considered_.begin(), considered_.end(), [&](int a, int b) {
    return RootScore(a, q, max_visits) > RootScore(b, q, max_visits);
  });
  considered_.resize((considered_.size() + 1) / 2);
}

int GumbelSearch::NextRootMove() {
  const int left = options_.num_simulations - num_simulations_;
  // A single legal move needs no search.
  if (left <= 0 || nodes_[0].moves.size() == 1) return -1;
  if (phase_.empty()) {
    if (!first_phase_ && considered_.size() > 1) Halve();
    first_phase_ = false;
    const int n = considered_.size();
    // The last phase, between the final two moves (or with one move left),
    // takes all the remaining simulations.
    const int visits =
        n <= 2 ? std::max(1, left / n)
               : std::max(1, options_.num_simulations / (num_phases_ * n));
    for (int v = 0; v < visits; ++v) {
      for (int move : considered_) phase_.push_back(move);
    }
  }
  const int move = phase_.front();
  phase_.pop_front();
  return move;
}

const Game* GumbelSearch::NextLeaf() {
  if (!nodes_[0].expanded) {
    if (nodes_[0].game.NumPossibleMoves() == 0) return nullptr;
    leaf_ = 0;
    path_.clear();
    return &nodes_[0].game;
  }
  while (true) {
    const int root_move = NextRootMove();
    if (root_move < 0) return nullptr;
    path_.assign(1, {0, root_move});
    while (true) {
      const auto [parent, move] = path_.back();
      int child = nodes_[parent].children[move];
      if (child < 0) {
        Game game = nodes_[parent].game;
        CHECK(game.MakeMove(nodes_[parent].moves[move]));
        child = AddNode(std::move(game));
        nodes_[parent].children[move] = child;
      }
      const Node& node = nodes_[child];
      if (node.finished) {
        Backup(node.value);
        break;
      }
      if (!node.expanded) {
        leaf_ = child;
        return &node.game;
      }
      // Deterministic selection: the move whose visit share lags its
      // improved policy the most.
      const std::vector<double> policy = Softmax(ImprovedLogits(node));
      int best = 0;
      double best_score = -std::numeric_limits<double>::infinity();
      for (size_t i = 0; i < policy.size(); ++i) {
        const double score = policy[i] - static_cast<double>(node.visits[i]) /
                                             (1 + node.total_visits);
        if (score > best_score) {
          best_score = score;
          best = i;
        }
      }
      path_.push_back({child, best});
    }
  }
}

void GumbelSearch::Expand(const Evaluation& evaluation) {
  Node& node = nodes_[leaf_];
  CHECK(!node.expanded);
  CHECK(!evaluation.moves.empty());
  node.expanded = true;
  node.moves = evaluation.moves;
  const int n = node.moves.size();
  node.priors.assign(evaluation.priors.begin(), evaluation.priors.end());
  node.logits.resize(n);
  for (int i = 0; i < n; ++i) {
    node.logits[i] = std::log(std::max(node.priors[i], 1e-30));
  }
  node.children.assign(n, -1);
  node.visits.assign(n, 0);
  node.value_sums.assign(n, 0);
  const double mover_value =
      evaluation.outcome[0] + 0.5 * evaluation.outcome[1];
  node.value = MoverValue(node, mover_value);

  if (leaf_ != 0) {
    Backup(node.value);
    return;
  }
  // The root: Gumbel top-k picks the moves to consider.
  std::extreme_value_distribution<double> gumbel;
  gumbels_.resize(n);
  for (double& g : gumbels_) g = options_.gumbel_scale * gumbel(rng_);
  considered_.resize(n);
  std::iota(considered_.begin(), considered_.end(), 0);
  std::stable_sort(considered_.begin(), considered_.end(), [&](int a, int b) {
    return gumbels_[a] + node.logits[a] > gumbels_[b] + node.logits[b];
  });
  considered_.resize(std::min(n, options_.num_considered));
  num_phases_ = std::max(
      1, static_cast<int>(std::ceil(std::log2(considered_.size()))));
}

void GumbelSearch::Backup(double value) {
  for (const auto& [node_index, move] : path_) {
    Node& node = nodes_[node_index];
    ++node.visits[move];
    node.value_sums[move] += value;
    ++node.total_visits;
  }
  ++num_simulations_;
}

SearchResult GumbelSearch::Result() const {
  const Node& root = nodes_[0];
  SearchResult result;
  if (!root.expanded) {
    result.move = Move::EmptyMove(root.game.current_color());
    return result;
  }
  result.moves = root.moves;
  result.visits = root.visits;
  result.num_simulations = num_simulations_;
  result.value = MixedValue(root);
  const std::vector<double> policy = Softmax(ImprovedLogits(root));
  result.policy.assign(policy.begin(), policy.end());
  const std::vector<double> q = CompletedQ(root);
  const int max_visits = MaxVisits(root);
  int best = considered_[0];
  for (int move : considered_) {
    if (RootScore(move, q, max_visits) > RootScore(best, q, max_visits)) {
      best = move;
    }
  }
  result.move = root.moves[best];
  return result;
}

SearchResult Search(const Game& game, const Evaluator& evaluator,
                    const SearchOptions& options, uint64_t seed) {
  GumbelSearch search(game, options, seed);
  while (const Game* leaf = search.NextLeaf()) {
    const Game* games[] = {leaf};
    search.Expand(evaluator.Evaluate(games)[0]);
  }
  return search.Result();
}

}  // namespace blokus
