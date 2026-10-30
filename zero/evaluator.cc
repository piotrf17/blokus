#include "zero/evaluator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "onnxruntime_cxx_api.h"

#include "zero/features.h"

namespace blokus {
namespace {

constexpr const char* kInputNames[] = {"planes", "globals"};
constexpr const char* kOutputNames[] = {"policy", "outcome", "margin"};
// The network predicts margins in units of 20 points.
constexpr float kMarginScale = 20;

Ort::Env& Env() {
  static Ort::Env* const env =
      new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "blokus");
  return *env;
}

Ort::Value Tensor(absl::Span<const float> data,
                  const std::vector<int64_t>& shape) {
  static const Ort::MemoryInfo memory_info =
      Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  // ONNX Runtime doesn't write to input tensors.
  return Ort::Value::CreateTensor<float>(
      memory_info, const_cast<float*>(data.data()), data.size(), shape.data(),
      shape.size());
}

// Checks that `session` has the inputs and outputs export.py writes.
absl::Status CheckSignature(const Ort::Session& session) {
  Ort::AllocatorWithDefaultOptions allocator;
  if (session.GetInputCount() != std::size(kInputNames) ||
      session.GetOutputCount() != std::size(kOutputNames)) {
    return absl::InvalidArgumentError("Wrong number of inputs or outputs");
  }
  for (size_t i = 0; i < std::size(kInputNames); ++i) {
    const std::string name = session.GetInputNameAllocated(i, allocator).get();
    if (name != kInputNames[i]) {
      return absl::InvalidArgumentError(
          absl::StrCat("Input ", i, " is ", name, ", not ", kInputNames[i]));
    }
  }
  for (size_t i = 0; i < std::size(kOutputNames); ++i) {
    const std::string name =
        session.GetOutputNameAllocated(i, allocator).get();
    if (name != kOutputNames[i]) {
      return absl::InvalidArgumentError(
          absl::StrCat("Output ", i, " is ", name, ", not ", kOutputNames[i]));
    }
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<Evaluator>> Evaluator::Load(
    const std::string& path, const Options& options) {
  CHECK_GE(options.num_threads, 1);
  // ONNX Runtime reports errors with exceptions.
  try {
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(options.num_threads);
    session_options.SetInterOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_ALL);
    auto session =
        std::make_unique<Ort::Session>(Env(), path.c_str(), session_options);
    if (absl::Status status = CheckSignature(*session); !status.ok()) {
      return status;
    }
    return std::unique_ptr<Evaluator>(new Evaluator(std::move(session)));
  } catch (const Ort::Exception& e) {
    return absl::InvalidArgumentError(
        absl::StrCat("Can't load ", path, ": ", e.what()));
  }
}

Evaluator::Evaluator(std::unique_ptr<Ort::Session> session)
    : session_(std::move(session)) {}

Evaluator::~Evaluator() = default;

void Evaluator::Run(int batch, absl::Span<const float> planes,
                    absl::Span<const float> globals, absl::Span<float> policy,
                    absl::Span<float> outcome,
                    absl::Span<float> margin) const {
  CHECK_GE(batch, 1);
  CHECK_EQ(planes.size(), batch * kPlanesSize);
  CHECK_EQ(globals.size(), batch * kNumGlobals);
  CHECK_EQ(policy.size(), batch * kPolicySize);
  CHECK_EQ(outcome.size(), batch * kNumOutcomes);
  CHECK_EQ(margin.size(), batch);
  const Ort::Value inputs[] = {
      Tensor(planes, {batch, kNumPlanes, Board::kNumRows, Board::kNumCols}),
      Tensor(globals, {batch, kNumGlobals}),
  };
  // The outputs are written straight into the caller's buffers.
  Ort::Value outputs[] = {
      Tensor(policy, {batch, kPolicySize}),
      Tensor(outcome, {batch, kNumOutcomes}),
      Tensor(margin, {batch, 1}),
  };
  try {
    session_->Run(Ort::RunOptions{nullptr}, kInputNames, inputs,
                  std::size(inputs), kOutputNames, outputs, std::size(outputs));
  } catch (const Ort::Exception& e) {
    LOG(FATAL) << "ONNX Runtime failed: " << e.what();
  }
}

std::vector<Evaluation> Evaluator::Evaluate(
    absl::Span<const Game* const> games) const {
  const int batch = games.size();
  std::vector<Evaluation> evaluations(batch);
  if (batch == 0) return evaluations;
  std::vector<float> planes(batch * kPlanesSize);
  std::vector<float> globals(batch * kNumGlobals);
  for (int i = 0; i < batch; ++i) {
    CHECK(!games[i]->Finished());
    EncodeInputs(*games[i],
                 absl::MakeSpan(planes).subspan(i * kPlanesSize, kPlanesSize),
                 absl::MakeSpan(globals).subspan(i * kNumGlobals, kNumGlobals));
  }
  std::vector<float> policy(batch * kPolicySize);
  std::vector<float> outcome(batch * kNumOutcomes);
  std::vector<float> margin(batch);
  Run(batch, planes, globals, absl::MakeSpan(policy), absl::MakeSpan(outcome),
      absl::MakeSpan(margin));

  for (int i = 0; i < batch; ++i) {
    Evaluation& evaluation = evaluations[i];
    // A softmax of the logits of the possible moves.
    evaluation.moves = games[i]->PossibleMoves();
    const float* logits = &policy[i * kPolicySize];
    evaluation.priors.reserve(evaluation.moves.size());
    float max_logit = -INFINITY;
    for (const Move& move : evaluation.moves) {
      evaluation.priors.push_back(logits[PolicyIndex(move)]);
      max_logit = std::max(max_logit, evaluation.priors.back());
    }
    float total = 0;
    for (float& prior : evaluation.priors) {
      prior = std::exp(prior - max_logit);
      total += prior;
    }
    for (float& prior : evaluation.priors) prior /= total;

    const float* outcome_logits = &outcome[i * kNumOutcomes];
    const float max_outcome =
        *std::max_element(outcome_logits, outcome_logits + kNumOutcomes);
    float outcome_total = 0;
    for (int j = 0; j < kNumOutcomes; ++j) {
      evaluation.outcome[j] = std::exp(outcome_logits[j] - max_outcome);
      outcome_total += evaluation.outcome[j];
    }
    for (float& p : evaluation.outcome) p /= outcome_total;

    evaluation.margin = margin[i] * kMarginScale;
  }
  return evaluations;
}

}  // namespace blokus
