// To run a benchmark:
//   $ bazel run -c opt ai:mcts_benchmark
//
// To profile a benchmark:
//   $ bazel build -c opt ai:mcts_benchmark
//   $ env CPUPROFILE=/tmp/mcts_benchmark.prof bazel-bin/ai/mcts_benchmark
//   $ google-pprof bazel-bin/ai/mcts_benchmark /tmp/mcts_benchmark.prof
#include "ai/mcts.h"

#include "benchmark/benchmark.h"

namespace blokus {
namespace {

// Results as of 2019/10/20:
// -----------------------------------------------------
// Benchmark           Time             CPU   Iterations
// -----------------------------------------------------
// BM_Rollout  115316947 ns    115194159 ns            6  (2019/10/20)
// BM_Rollout    4564837 ns      4560928 ns          152  (Slots + Corners)
// BM_Rollout    4244384 ns      4244421 ns          164  (Bitmaps w/o dup fix)
// BM_Rollout    4501754 ns      4501773 ns          155  (Bitmaps w/ dup fix)
// BM_Rollout    4154011 ns      4154033 ns          169  (Remove std::map)
// BM_Rollout    2510316 ns      2510315 ns          276  (opt CornerFitsSlot)

static void BM_Rollout(benchmark::State& state) {
  Game game(4);
  for (auto _ : state) {
    Rollout(game);
  }
}
BENCHMARK(BM_Rollout);

}  // namespace
}  // namespace blokus

BENCHMARK_MAIN();
