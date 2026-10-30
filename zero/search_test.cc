#include "zero/search.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <numeric>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "game/board.h"
#include "game/game.h"
#include "zero/evaluator.h"

namespace blokus {
namespace {

using ::testing::Each;
using ::testing::Eq;
using ::testing::FloatNear;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::Le;
using ::testing::SizeIs;

// The win probability of a position for its mover's player.
using WinFunction = std::function<float(const Game&)>;

// Uniform priors, and a win probability from `win` (no draws).
Evaluation FakeEvaluation(const Game& game, const WinFunction& win) {
  Evaluation evaluation;
  evaluation.moves = game.PossibleMoves();
  evaluation.priors.assign(evaluation.moves.size(),
                           1.0f / evaluation.moves.size());
  const float p = win(game);
  evaluation.outcome = {p, 0, 1 - p};
  evaluation.margin = 0;
  return evaluation;
}

SearchResult FakeSearch(const Game& game, const SearchOptions& options,
                        const WinFunction& win, uint64_t seed = 1) {
  GumbelSearch search(game, options, seed);
  while (const Game* leaf = search.NextLeaf()) {
    search.Expand(FakeEvaluation(*leaf, win));
  }
  return search.Result();
}

float EvenGame(const Game&) { return 0.5f; }

// A 2-player game after `num_moves` moves.
Game GameAfter(int num_moves) {
  Game game(2);
  for (int i = 0; i < num_moves; ++i) {
    CHECK(game.MakeMove(game.PossibleMove(game.NumPossibleMoves() / 3)));
  }
  return game;
}

TEST(GumbelSearchTest, SpendsTheSimulations) {
  const Game game = GameAfter(4);
  const SearchResult result =
      FakeSearch(game, {.num_simulations = 50}, EvenGame);
  EXPECT_THAT(result.num_simulations, Eq(50));
  EXPECT_THAT(std::accumulate(result.visits.begin(), result.visits.end(), 0),
              Eq(50));
  EXPECT_THAT(result.moves, Eq(game.PossibleMoves()));
  EXPECT_THAT(result.policy, SizeIs(result.moves.size()));
  EXPECT_THAT(std::accumulate(result.policy.begin(), result.policy.end(), 0.0),
              testing::DoubleNear(1, 1e-5));
  EXPECT_THAT(result.value, FloatNear(0.5, 1e-5));
}

// Sequential halving: 16 moves get a visit each, then 8, then 4, and the
// last two share what's left.
TEST(GumbelSearchTest, SequentialHalving) {
  const SearchResult result = FakeSearch(
      GameAfter(4), {.num_simulations = 64, .num_considered = 16}, EvenGame);
  std::vector<int> visits = result.visits;
  std::sort(visits.rbegin(), visits.rend());
  // 64 / (4 phases x 16) = 1, 64 / (4 x 8) = 2, 64 / (4 x 4) = 4: 16 + 16 +
  // 16 simulations, then the last 16 split between two moves.
  EXPECT_THAT(std::vector<int>(visits.begin(), visits.begin() + 16),
              Eq(std::vector<int>{15, 15, 7, 7, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1,
                                  1, 1}));
  EXPECT_THAT(std::vector<int>(visits.begin() + 16, visits.end()), Each(0));
}

// Without simulations, the improved policy is the prior.
TEST(GumbelSearchTest, NoSimulationsKeepsThePrior) {
  const SearchResult result =
      FakeSearch(GameAfter(4), {.num_simulations = 0}, EvenGame);
  EXPECT_THAT(result.num_simulations, Eq(0));
  EXPECT_THAT(result.policy, Each(FloatNear(1.0f / result.moves.size(),
                                            1e-6)));
}

// The search finds the one root move that wins, for either player to move.
class FindsWinningMoveTest : public testing::TestWithParam<int> {};

TEST_P(FindsWinningMoveTest, FindsWinningMove) {
  const Game game = GameAfter(GetParam());
  const int root_player = game.current_player();
  const std::vector<Move> moves = game.PossibleMoves();
  const Move winning = moves[moves.size() / 2];
  const size_t num_moves = game.moves().size();
  // The winning move wins for the root's player whoever is to move later.
  auto win = [&](const Game& position) {
    if (position.moves().size() == num_moves) return 0.5f;
    const bool good = position.moves()[num_moves] == winning;
    const float root_win = good ? 0.95f : 0.3f;
    return position.current_player() == root_player ? root_win
                                                    : 1 - root_win;
  };
  // Every root move is considered, with enough simulations to try each.
  const SearchResult result = FakeSearch(
      game,
      {.num_simulations = 4 * static_cast<int>(moves.size()),
       .num_considered = static_cast<int>(moves.size())},
      win);
  EXPECT_THAT(result.move, Eq(winning));
  const auto best = std::max_element(result.policy.begin(),
                                     result.policy.end());
  EXPECT_THAT(result.moves[best - result.policy.begin()], Eq(winning));
}

INSTANTIATE_TEST_SUITE_P(BothPlayers, FindsWinningMoveTest,
                         testing::Values(4, 5));

TEST(GumbelSearchTest, SameSeedSameSearch) {
  auto win = [](const Game& game) {
    return 0.5f + 0.01f * (game.moves().size() % 7);
  };
  const Game game = GameAfter(6);
  const SearchResult a = FakeSearch(game, {}, win, /*seed=*/3);
  const SearchResult b = FakeSearch(game, {}, win, /*seed=*/3);
  EXPECT_THAT(a.move, Eq(b.move));
  EXPECT_THAT(a.visits, Eq(b.visits));
  EXPECT_THAT(a.policy, Eq(b.policy));
}

TEST(GumbelSearchTest, PassesWithoutMoves) {
  // BLUE passes, so it has to pass again on its next turn.
  Game game(2);
  CHECK(game.MakeMove(Move::EmptyMove(BLUE)));
  for (int i = 0; i < 3; ++i) CHECK(game.MakeMove(game.PossibleMove(0)));
  const SearchResult result = FakeSearch(game, {}, EvenGame);
  EXPECT_THAT(result.move, Eq(Move::EmptyMove(BLUE)));
  EXPECT_THAT(result.moves, SizeIs(0));
}

// Late in a game, simulations reach finished games and forced passes. The
// network always says 0.5, so values only move when results are reached.
TEST(GumbelSearchTest, PlaysToTheEnd) {
  Game game(2);
  int num_searches = 0;
  int num_decided = 0;
  while (!game.Finished()) {
    const SearchResult result = FakeSearch(
        game, {.num_simulations = 16}, EvenGame, /*seed=*/num_searches);
    EXPECT_THAT(result.value, Ge(0.0));
    EXPECT_THAT(result.value, Le(1.0));
    num_decided += result.value != 0.5f && result.num_simulations > 0;
    ASSERT_TRUE(game.MakeMove(result.move)) << result.move.DebugString();
    ++num_searches;
  }
  EXPECT_THAT(num_searches, Gt(40));
  EXPECT_THAT(num_decided, Gt(0));
}

TEST(SearchTest, RunsWithANetwork) {
  absl::StatusOr<std::unique_ptr<Evaluator>> evaluator =
      Evaluator::Load("zero/testdata/tiny.onnx", {});
  ASSERT_TRUE(evaluator.ok()) << evaluator.status();
  const Game game = GameAfter(4);
  const SearchResult result =
      Search(game, **evaluator, {.num_simulations = 20}, /*seed=*/1);
  EXPECT_THAT(result.num_simulations, Eq(20));
  Game next = game;
  EXPECT_TRUE(next.MakeMove(result.move));
}

}  // namespace
}  // namespace blokus
