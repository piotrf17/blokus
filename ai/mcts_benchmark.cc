#include "ai/mcts.h"

#include "benchmark/benchmark.h"

namespace blokus {
namespace {

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
