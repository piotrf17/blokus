#include "zero/zero_ai.h"

#include <memory>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "ai/random.h"
#include "game/game_runner.h"
#include "zero/evaluator.h"

namespace blokus {
namespace {

using ::testing::Gt;

TEST(ZeroAITest, PlaysFullGame) {
  absl::StatusOr<std::unique_ptr<Evaluator>> evaluator =
      Evaluator::Load("zero/testdata/tiny.onnx", {});
  CHECK_OK(evaluator.status());
  GameRunner runner(2);
  runner.AddPlayer(std::make_unique<ZeroAI>(
      0, evaluator->get(), SearchOptions{.num_simulations = 8}, /*seed=*/1));
  runner.AddPlayer(std::make_unique<RandomAI>(1, /*seed=*/1));
  int num_moves = 0;
  runner.AddObserver([&](const Game&, const Move&) { ++num_moves; });
  runner.Play();
  EXPECT_THAT(num_moves, Gt(40));
}

}  // namespace
}  // namespace blokus
