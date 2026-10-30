// Generates BlokusZero training data from MCTS self-play: every position
// where the color to move has a choice becomes a TrainingExample (see
// zero/training.proto),
// with the root's visit counts as the policy target. Games run in parallel,
// one per thread, each with single-threaded MCTS players. Every
// --games_per_shard games, the examples go to a new shard,
// <output>-00000.bagz and so on.
//
//   bazel run -c opt //zero:generate -- --num_games=1000 \
//     --output=$PWD/zero/data/bootstrap/mcts1k-s0 --seed=0

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

#include "ai/mcts.h"
#include "game/game.h"
#include "zero/shard.h"
#include "zero/training.pb.h"

ABSL_FLAG(std::string, output, "",
          "Path prefix of the shards, which get a number and extension.");
ABSL_FLAG(int, games_per_shard, 500, "Games in each shard.");
ABSL_FLAG(int, num_games, 100, "Number of games to play.");
ABSL_FLAG(int, num_players, 2, "Number of players, 2 or 4.");
ABSL_FLAG(int, iterations, 1000, "MCTS iterations per move.");
ABSL_FLAG(int, sampled_moves, 8,
          "Moves at the start of each game that are sampled in proportion "
          "to visits, to vary the openings.");
ABSL_FLAG(int, threads, 8, "Games to play at once.");
ABSL_FLAG(int64_t, seed, -1,
          "Random number seed. If -1, seeds from std::random_device.");

namespace blokus {
namespace {

// Plays one game, and returns an example for every move with a choice.
std::vector<TrainingExample> PlayGame(int num_players,
                                      const MctsOptions& options,
                                      std::mt19937_64& rng) {
  std::vector<std::unique_ptr<MctsAI>> players;
  for (int id = 0; id < num_players; ++id) {
    MctsOptions player_options = options;
    player_options.seed = rng();
    players.push_back(std::make_unique<MctsAI>(id, player_options));
  }
  Game game(num_players);
  std::vector<TrainingExample> examples;
  while (!game.Finished()) {
    MctsAI& player = *players[game.current_player()];
    const Move move = player.SelectMove(game);
    const std::vector<MoveStats>& stats = player.root_stats();
    if (stats.size() > 1) {
      std::vector<Move> moves;
      std::vector<double> visits;
      for (const MoveStats& s : stats) {
        moves.push_back(s.move);
        visits.push_back(s.visits);
      }
      examples.push_back(MakeExample(game, moves, visits));
    }
    CHECK(game.MakeMove(move)) << move.DebugString();
  }
  const GameResult result = game.Result();
  for (TrainingExample& example : examples) SetResult(result, example);
  return examples;
}

int Main() {
  const std::string output = absl::GetFlag(FLAGS_output);
  CHECK(!output.empty()) << "--output is required";
  const int num_games = absl::GetFlag(FLAGS_num_games);
  const int num_players = absl::GetFlag(FLAGS_num_players);
  CHECK(num_players == 2 || num_players == 4);
  const MctsOptions options = {
      .num_iterations = absl::GetFlag(FLAGS_iterations),
      .num_sampled_moves = absl::GetFlag(FLAGS_sampled_moves),
  };
  std::mt19937_64 seeder(absl::GetFlag(FLAGS_seed) != -1
                             ? absl::GetFlag(FLAGS_seed)
                             : std::random_device()());

  const int games_per_shard = absl::GetFlag(FLAGS_games_per_shard);
  CHECK_GT(games_per_shard, 0);

  std::mutex mutex;
  // The finished games' examples that aren't in a shard yet.
  std::vector<TrainingExample> pending;
  int num_pending_games = 0;
  int num_shards = 0;
  const auto write_shard = [&] {
    const std::string path =
        absl::StrFormat("%s-%05d.bagz", output, num_shards++);
    CHECK_OK(WriteShard(path, pending));
    LOG(INFO) << "Wrote " << pending.size() << " examples to " << path;
    pending.clear();
    num_pending_games = 0;
  };
  int64_t num_examples = 0;
  int num_done = 0;
  std::atomic<int> next_game = 0;
  const absl::Time start = absl::Now();
  std::vector<std::thread> threads;
  for (int t = 0; t < absl::GetFlag(FLAGS_threads); ++t) {
    threads.emplace_back([&, seed = seeder()] {
      std::mt19937_64 rng(seed);
      while (next_game.fetch_add(1) < num_games) {
        const std::vector<TrainingExample> examples =
            PlayGame(num_players, options, rng);
        std::lock_guard<std::mutex> lock(mutex);
        pending.insert(pending.end(), examples.begin(), examples.end());
        if (++num_pending_games == games_per_shard) write_shard();
        num_examples += examples.size();
        ++num_done;
        const double minutes = absl::ToDoubleMinutes(absl::Now() - start);
        LOG(INFO) << absl::StrFormat(
            "%d/%d games, %d examples, %.1f games/hour", num_done, num_games,
            num_examples, num_done / minutes * 60);
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (num_pending_games > 0) write_shard();
  return 0;
}

}  // namespace
}  // namespace blokus

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  return blokus::Main();
}
