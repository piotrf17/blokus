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
//
// Run on (4 X 3400 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x2)
//   L1 Instruction 32K (x2)
//   L2 Unified 256K (x2)
//   L3 Unified 4096K (x1)
// Load Average: 1.14, 1.01, 0.99
// -----------------------------------------------------
// Benchmark           Time             CPU   Iterations
// -----------------------------------------------------
// BM_Rollout  115316947 ns    115194159 ns            6
static void BM_Rollout(benchmark::State& state) {
  Game game;
  for (auto _ : state) {
    Rollout(game);
  }
}
BENCHMARK(BM_Rollout);

}  // namespace
}  // namespace blokus

BENCHMARK_MAIN();
