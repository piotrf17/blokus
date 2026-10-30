#include "zero/policy_ai.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "ai/random.h"
#include "game/board.h"
#include "game/game.h"
#include "game/game_runner.h"
#include "zero/evaluator.h"

namespace blokus {
namespace {

using ::testing::Eq;
using ::testing::Gt;

// A 2 x 8 network with random weights, from zero/train/make_testdata.py.
std::unique_ptr<Evaluator> LoadTiny() {
  absl::StatusOr<std::unique_ptr<Evaluator>> evaluator =
      Evaluator::Load("zero/testdata/tiny.onnx", {});
  CHECK_OK(evaluator.status());
  return *std::move(evaluator);
}

TEST(PolicyAITest, PlaysTheLargestPrior) {
  std::unique_ptr<Evaluator> evaluator = LoadTiny();
  PolicyAI ai(0, evaluator.get());
  Game game(2);
  for (int i = 0; i < 6; ++i) {
    const Game* games[] = {&game};
    const Evaluation evaluation = evaluator->Evaluate(games)[0];
    const float best = *std::max_element(evaluation.priors.begin(),
                                         evaluation.priors.end());
    const Move move = ai.SelectMove(game);
    const auto it = std::find(evaluation.moves.begin(), evaluation.moves.end(),
                              move);
    ASSERT_TRUE(it != evaluation.moves.end()) << move.DebugString();
    EXPECT_THAT(evaluation.priors[it - evaluation.moves.begin()], Eq(best));
    ASSERT_TRUE(game.MakeMove(move));
  }
}

TEST(PolicyAITest, PassesWithoutMoves) {
  std::unique_ptr<Evaluator> evaluator = LoadTiny();
  PolicyAI ai(0, evaluator.get());
  // BLUE has passed, so it has to pass again.
  Game game(4);
  CHECK(game.MakeMove(Move::EmptyMove(BLUE)));
  for (int i = 0; i < 3; ++i) CHECK(game.MakeMove(game.PossibleMove(0)));
  EXPECT_THAT(ai.SelectMove(game), Eq(Move::EmptyMove(BLUE)));
}

TEST(PolicyAITest, PlaysFullGame) {
  std::unique_ptr<Evaluator> evaluator = LoadTiny();
  GameRunner runner(2);
  runner.AddPlayer(std::make_unique<PolicyAI>(0, evaluator.get()));
  runner.AddPlayer(std::make_unique<RandomAI>(1, /*seed=*/1));
  int num_moves = 0;
  runner.AddObserver([&](const Game&, const Move&) { ++num_moves; });
  runner.Play();
  EXPECT_THAT(num_moves, Gt(40));
}

}  // namespace
}  // namespace blokus
