#ifndef BLOKUS_ZERO_EVALUATOR_H_
#define BLOKUS_ZERO_EVALUATOR_H_

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/span.h"

#include "game/board.h"
#include "game/game.h"
#include "zero/features.h"

namespace Ort {
struct Session;
}

namespace blokus {

// The network's view of a position, for the current color of a game.
struct Evaluation {
  // The current color's possible moves (Game::PossibleMoves), and the
  // network's probability for each. Both are empty if it has to pass.
  std::vector<Move> moves;
  std::vector<float> priors;
  // Win, draw and loss probabilities for the current player.
  std::array<float, kNumOutcomes> outcome;
  // The current player's predicted final score margin over the best other
  // player, in points.
  float margin;
};

// Runs a BlokusZero network exported by zero/train/export.py, with ONNX
// Runtime on the CPU. Thread-safe: several threads can evaluate at once.
class Evaluator {
 public:
  struct Options {
    // Threads ONNX Runtime uses within one evaluation. 1 runs it on the
    // calling thread.
    int num_threads = 1;
  };

  // Loads the ONNX model at `path`.
  static absl::StatusOr<std::unique_ptr<Evaluator>> Load(
      const std::string& path, const Options& options);

  ~Evaluator();

  // Evaluates the positions, in one batch. The games must not be finished.
  std::vector<Evaluation> Evaluate(absl::Span<const Game* const> games) const;

  // Runs the network on `batch` encoded positions (see EncodeInputs), with
  // `planes` and `globals` holding batch * kPlanesSize and
  // batch * kNumGlobals floats. Writes batch * kPolicySize policy logits,
  // batch * kNumOutcomes outcome logits, and batch margins (in units of 20
  // points).
  void Run(int batch, absl::Span<const float> planes,
           absl::Span<const float> globals, absl::Span<float> policy,
           absl::Span<float> outcome, absl::Span<float> margin) const;

 private:
  explicit Evaluator(std::unique_ptr<Ort::Session> session);

  std::unique_ptr<Ort::Session> session_;
};

}  // namespace blokus

#endif
