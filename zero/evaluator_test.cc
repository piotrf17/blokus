#include "zero/evaluator.h"

#include <cmath>
#include <fstream>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "game/board.h"
#include "game/game.h"
#include "zero/features.h"

namespace blokus {
namespace {

using ::testing::DoubleNear;
using ::testing::Eq;
using ::testing::FloatNear;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

// Written by zero/train/make_testdata.py: a 2 x 8 network, and its outputs
// for TestInputs computed by PyTorch.
constexpr char kModel[] = "zero/testdata/tiny.onnx";
constexpr char kExpected[] = "zero/testdata/tiny_expected.txt";
constexpr int kBatch = 3;

std::unique_ptr<Evaluator> LoadTiny() {
  absl::StatusOr<std::unique_ptr<Evaluator>> evaluator =
      Evaluator::Load(kModel, {});
  CHECK_OK(evaluator.status());
  return *std::move(evaluator);
}

// The same inputs as test_inputs() in make_testdata.py.
void TestInputs(std::vector<float>& planes, std::vector<float>& globals) {
  planes.assign(kBatch * kPlanesSize, 0);
  globals.assign(kBatch * kNumGlobals, 0);
  for (int b = 0; b < kBatch; ++b) {
    for (int i = 0; i < kPlanesSize; ++i) {
      planes[b * kPlanesSize + i] = (b * 7 + i * 13) % 5 == 0;
    }
    for (int i = 0; i < kNumGlobals; ++i) {
      globals[b * kNumGlobals + i] = (b + i * 3) % 4 == 0;
    }
  }
}

TEST(EvaluatorTest, MatchesPyTorch) {
  std::unique_ptr<Evaluator> evaluator = LoadTiny();
  std::vector<float> planes, globals;
  TestInputs(planes, globals);
  std::vector<float> policy(kBatch * kPolicySize);
  std::vector<float> outcome(kBatch * kNumOutcomes);
  std::vector<float> margin(kBatch);
  evaluator->Run(kBatch, planes, globals, absl::MakeSpan(policy),
                 absl::MakeSpan(outcome), absl::MakeSpan(margin));

  std::ifstream expected(kExpected);
  ASSERT_TRUE(expected.good());
  std::string line;
  int num_checked = 0;
  while (std::getline(expected, line)) {
    std::istringstream in(line);
    std::string kind;
    int b;
    in >> kind >> b;
    SCOPED_TRACE(line);
    if (kind == "policy_sum") {
      double value;
      in >> value;
      const float* begin = &policy[b * kPolicySize];
      EXPECT_THAT(std::accumulate(begin, begin + kPolicySize, 0.0),
                  DoubleNear(value, 1e-2));
    } else if (kind == "policy") {
      int index;
      float value;
      in >> index >> value;
      EXPECT_THAT(policy[b * kPolicySize + index], FloatNear(value, 1e-4));
    } else if (kind == "outcome") {
      for (int i = 0; i < kNumOutcomes; ++i) {
        float value;
        in >> value;
        EXPECT_THAT(outcome[b * kNumOutcomes + i], FloatNear(value, 1e-4));
      }
    } else if (kind == "margin") {
      float value;
      in >> value;
      EXPECT_THAT(margin[b], FloatNear(value, 1e-4));
    } else {
      ADD_FAILURE() << "Unknown line";
    }
    ++num_checked;
  }
  // policy_sum, 4 policy logits, outcome and margin for each position.
  EXPECT_THAT(num_checked, Eq(kBatch * 7));
}

TEST(EvaluatorTest, LoadFailsForMissingFile) {
  EXPECT_THAT(Evaluator::Load("zero/testdata/missing.onnx", {}).ok(),
              Eq(false));
}

// A few positions: the opening, a 2-player game some moves in, and a 4-player
// game where BLUE has passed and has to pass again.
std::vector<Game> TestGames() {
  std::vector<Game> games;
  games.emplace_back(2);
  Game middle(2);
  for (int i = 0; i < 12; ++i) {
    CHECK(middle.MakeMove(middle.PossibleMove(middle.NumPossibleMoves() / 3)));
  }
  games.push_back(middle);
  Game passed(4);
  CHECK(passed.MakeMove(Move::EmptyMove(BLUE)));
  for (int i = 0; i < 3; ++i) CHECK(passed.MakeMove(passed.PossibleMove(0)));
  games.push_back(passed);
  return games;
}

TEST(EvaluatorTest, EvaluateGivesDistributions) {
  std::unique_ptr<Evaluator> evaluator = LoadTiny();
  const std::vector<Game> games = TestGames();
  std::vector<const Game*> pointers;
  for (const Game& game : games) pointers.push_back(&game);
  const std::vector<Evaluation> evaluations = evaluator->Evaluate(pointers);
  ASSERT_THAT(evaluations, SizeIs(games.size()));
  for (int i = 0; i < static_cast<int>(games.size()); ++i) {
    SCOPED_TRACE(i);
    const Evaluation& evaluation = evaluations[i];
    EXPECT_THAT(evaluation.moves, Eq(games[i].PossibleMoves()));
    ASSERT_THAT(evaluation.priors, SizeIs(evaluation.moves.size()));
    if (!evaluation.priors.empty()) {
      EXPECT_THAT(std::accumulate(evaluation.priors.begin(),
                                  evaluation.priors.end(), 0.0),
                  DoubleNear(1, 1e-4));
    }
    for (float prior : evaluation.priors) EXPECT_THAT(prior, Ge(0));
    EXPECT_THAT(evaluation.outcome[0] + evaluation.outcome[1] +
                    evaluation.outcome[2],
                FloatNear(1, 1e-5));
  }
  EXPECT_THAT(evaluations[0].moves, Not(IsEmpty()));
  EXPECT_THAT(evaluations[2].moves, IsEmpty());
}

// The priors are the softmax of the moves' policy logits, and the outcome
// and margin come from the network's outputs.
TEST(EvaluatorTest, EvaluateMatchesRun) {
  std::unique_ptr<Evaluator> evaluator = LoadTiny();
  const Game game = TestGames()[1];
  const std::vector<Evaluation> evaluations = evaluator->Evaluate({&game});

  std::vector<float> planes(kPlanesSize);
  std::vector<float> globals(kNumGlobals);
  EncodeInputs(game, absl::MakeSpan(planes), absl::MakeSpan(globals));
  std::vector<float> policy(kPolicySize);
  std::vector<float> outcome(kNumOutcomes);
  std::vector<float> margin(1);
  evaluator->Run(1, planes, globals, absl::MakeSpan(policy),
                 absl::MakeSpan(outcome), absl::MakeSpan(margin));

  const Evaluation& evaluation = evaluations[0];
  ASSERT_THAT(evaluation.moves, SizeIs(Ge(2)));
  const float first_logit = policy[PolicyIndex(evaluation.moves[0])];
  for (int i = 1; i < static_cast<int>(evaluation.moves.size()); ++i) {
    const float logit = policy[PolicyIndex(evaluation.moves[i])];
    EXPECT_THAT(evaluation.priors[i] / evaluation.priors[0],
                FloatNear(std::exp(logit - first_logit), 1e-4));
  }
  const float win_over_loss = std::exp(outcome[0] - outcome[2]);
  EXPECT_THAT(evaluation.outcome[0] / evaluation.outcome[2],
              FloatNear(win_over_loss, 1e-4));
  EXPECT_THAT(evaluation.margin, FloatNear(margin[0] * 20, 1e-4));
}

// A batch gives the same results as evaluating one game at a time.
TEST(EvaluatorTest, BatchMatchesSingle) {
  std::unique_ptr<Evaluator> evaluator = LoadTiny();
  const std::vector<Game> games = TestGames();
  std::vector<const Game*> pointers;
  for (const Game& game : games) pointers.push_back(&game);
  const std::vector<Evaluation> batch = evaluator->Evaluate(pointers);
  for (int i = 0; i < static_cast<int>(games.size()); ++i) {
    const Evaluation single = evaluator->Evaluate({&games[i]})[0];
    ASSERT_THAT(single.priors, SizeIs(batch[i].priors.size()));
    for (int j = 0; j < static_cast<int>(single.priors.size()); ++j) {
      EXPECT_THAT(single.priors[j], FloatNear(batch[i].priors[j], 1e-5));
    }
    for (int j = 0; j < kNumOutcomes; ++j) {
      EXPECT_THAT(single.outcome[j], FloatNear(batch[i].outcome[j], 1e-5));
    }
    EXPECT_THAT(single.margin, FloatNear(batch[i].margin, 1e-4));
  }
}

}  // namespace
}  // namespace blokus
