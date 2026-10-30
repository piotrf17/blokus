// Measures BlokusZero network evaluations per second, by batch size and
// number of threads. Export models with zero/train/export.py, then:
//
//   bazel run -c opt //zero:evaluator_benchmark -- \
//     --model=$PWD/zero/data/models/random_6x32.onnx
//
// Each thread evaluates its own batches of positions with a shared
// Evaluator. "run" times only the network; "evaluate" adds encoding the
// inputs, listing the possible moves, and the softmax over them
// (Evaluator::Evaluate).

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/types/span.h"

#include "game/game.h"
#include "zero/evaluator.h"
#include "zero/features.h"

ABSL_FLAG(std::string, model, "", "ONNX model to benchmark.");
ABSL_FLAG(std::string, batch_sizes, "1,8,32,128", "Batch sizes to try.");
ABSL_FLAG(std::string, threads, "1,4,8", "Thread counts to try.");
ABSL_FLAG(int, intra_op_threads, 1, "ONNX Runtime threads per evaluation.");
ABSL_FLAG(double, seconds, 3, "How long to measure each setting.");

namespace blokus {
namespace {

std::vector<int> ParseInts(const std::string& list) {
  std::vector<int> values;
  for (absl::string_view part : absl::StrSplit(list, ',')) {
    int value;
    CHECK(absl::SimpleAtoi(part, &value)) << part;
    values.push_back(value);
  }
  return values;
}

// Positions from random 2-player games, at random points where the color to
// move has moves.
std::vector<Game> RandomPositions(int count) {
  std::mt19937 rng(0);
  std::vector<Game> positions;
  while (static_cast<int>(positions.size()) < count) {
    Game game(2);
    const int stop = std::uniform_int_distribution<int>(0, 60)(rng);
    for (int i = 0; i < stop && !game.Finished(); ++i) {
      const int n = game.NumPossibleMoves();
      Move move = Move::EmptyMove(game.current_color());
      if (n > 0) {
        std::uniform_int_distribution<int> pick(0, n - 1);
        move = game.PossibleMove(pick(rng));
      }
      CHECK(game.MakeMove(move));
    }
    if (!game.Finished() && game.NumPossibleMoves() > 0) {
      positions.push_back(game);
    }
  }
  return positions;
}

// Runs `work(thread, batch)` on `num_threads` threads for --seconds, where
// each call handles `batch` positions. Returns positions per second.
template <typename Work>
double Measure(int num_threads, int batch, Work work) {
  std::atomic<bool> stop = false;
  std::atomic<int64_t> done = 0;
  std::vector<std::thread> threads;
  const auto start = std::chrono::steady_clock::now();
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t] {
      int64_t count = 0;
      while (!stop) {
        work(t, batch);
        count += batch;
      }
      done += count;
    });
  }
  std::this_thread::sleep_for(
      std::chrono::duration<double>(absl::GetFlag(FLAGS_seconds)));
  stop = true;
  for (std::thread& thread : threads) thread.join();
  const std::chrono::duration<double> elapsed =
      std::chrono::steady_clock::now() - start;
  return done / elapsed.count();
}

int Main() {
  absl::StatusOr<std::unique_ptr<Evaluator>> loaded = Evaluator::Load(
      absl::GetFlag(FLAGS_model),
      {.num_threads = absl::GetFlag(FLAGS_intra_op_threads)});
  CHECK_OK(loaded.status());
  const Evaluator& evaluator = **loaded;
  const std::vector<int> batch_sizes =
      ParseInts(absl::GetFlag(FLAGS_batch_sizes));
  const std::vector<int> thread_counts =
      ParseInts(absl::GetFlag(FLAGS_threads));
  int max_batch = 0;
  for (int b : batch_sizes) max_batch = std::max(max_batch, b);
  const std::vector<Game> positions = RandomPositions(max_batch);

  std::vector<const Game*> games;
  for (const Game& game : positions) games.push_back(&game);
  std::vector<float> planes(max_batch * kPlanesSize);
  std::vector<float> globals(max_batch * kNumGlobals);
  for (int i = 0; i < max_batch; ++i) {
    EncodeInputs(positions[i],
                 absl::MakeSpan(planes).subspan(i * kPlanesSize, kPlanesSize),
                 absl::MakeSpan(globals).subspan(i * kNumGlobals, kNumGlobals));
  }

  std::cout << absl::StrFormat("%s, %d intra-op threads\n",
                               absl::GetFlag(FLAGS_model),
                               absl::GetFlag(FLAGS_intra_op_threads));
  std::cout << "threads  batch      run/s  evaluate/s\n";
  for (int num_threads : thread_counts) {
    for (int batch : batch_sizes) {
      // Output buffers for each thread.
      std::vector<std::vector<float>> policy(
          num_threads, std::vector<float>(batch * kPolicySize));
      std::vector<std::vector<float>> outcome(
          num_threads, std::vector<float>(batch * kNumOutcomes));
      std::vector<std::vector<float>> margin(num_threads,
                                             std::vector<float>(batch));
      const double run = Measure(num_threads, batch, [&](int t, int b) {
        evaluator.Run(b, absl::MakeSpan(planes).first(b * kPlanesSize),
                      absl::MakeSpan(globals).first(b * kNumGlobals),
                      absl::MakeSpan(policy[t]), absl::MakeSpan(outcome[t]),
                      absl::MakeSpan(margin[t]));
      });
      const double evaluate = Measure(num_threads, batch, [&](int, int b) {
        evaluator.Evaluate(absl::MakeSpan(games).first(b));
      });
      std::cout << absl::StrFormat("%7d  %5d  %9.0f  %10.0f\n", num_threads,
                                   batch, run, evaluate);
    }
  }
  return 0;
}

}  // namespace
}  // namespace blokus

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  return blokus::Main();
}
