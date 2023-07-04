// To run a benchmark:
//   $ bazel run -c opt ai:mcts_benchmark
//
// To profile a benchmark:
//   $ bazel build -c opt ai:mcts_benchmark
//   $ env CPUPROFILE=/tmp/mcts_benchmark.prof bazel-bin/ai/mcts_benchmark
//   $ pprof -http=":8000" bazel-bin/ai/mcts_benchmark /tmp/mcts_benchmark.prof
#include "ai/mcts.h"

#include "benchmark/benchmark.h"

namespace blokus {
namespace {

// -----------------------------------------------------
// Benchmark           Time             CPU   Iterations
// -----------------------------------------------------
// BM_Rollout  115316947 ns    115194159 ns            6  (2019/10/20)
// BM_Rollout    4564837 ns      4560928 ns          152  (Slots + Corners)
// BM_Rollout    4244384 ns      4244421 ns          164  (Bitmaps w/o dup fix)
// BM_Rollout    4501754 ns      4501773 ns          155  (Bitmaps w/ dup fix)
// BM_Rollout    4154011 ns      4154033 ns          169  (Remove std::map)
// BM_Rollout    2510316 ns      2510315 ns          276  (opt CornerFitsSlot)
// BM_Rollout    2072928 ns      2071352 ns          334  (remove hashmap)
// BM_Rollout     932193 ns       932172 ns          728  (possible tile cache)

static void BM_Rollout(benchmark::State& state) {
  for (auto _ : state) {
    Game game(4);
    Rollout(game);
  }
}
BENCHMARK(BM_Rollout);

static void BM_SelectMove(benchmark::State& state) {
  for (auto _ : state) {
    Game game(4);
    MctsOptions options{
      .num_iterations = state.range(0),
      .num_threads = 8,
    };
    MctsAI ai(0, options);
    ai.SelectMove(game);
    state.SetItemsProcessed(state.range(0));
  }      
}
BENCHMARK(BM_SelectMove)->Arg(10000);

}  // namespace
}  // namespace blokus

BENCHMARK_MAIN();
