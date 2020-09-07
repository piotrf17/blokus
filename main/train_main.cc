#include "absl/memory/memory.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "ai/mcts.h"
#include "ai/random.h"
#include "game/game_runner.h"

DEFINE_int32(seed, -1, "Random number seed. If -1, use time.");
DEFINE_int32(num_games, 10, "Number of games to play.");
DEFINE_bool(print_board, false, "Print the board during play.");

DEFINE_int32(num_players, 2, "Number of players, 2 or 4.");
DEFINE_int32(num_mcts_iterations, 10000,
             "Number of MCTS iterations to run per move.");
DEFINE_int32(num_mcts_rollouts, 1,
             "Number of MCTS rollouts per iterations.");
DEFINE_int32(num_mcts_threads, 1, "Number of MCTS threads.");

int main(int argc, char **argv) {
  // Initialize command line flags and logging.
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  CHECK(FLAGS_num_players == 2 || FLAGS_num_players == 4);

  if (FLAGS_seed != -1) {
    srand(FLAGS_seed);
  } else {
    srand(time(NULL));
  }

  std::vector<int> total_scores(FLAGS_num_players, 0);

  absl::Time start = absl::Now();
  for (int i = 0; i < FLAGS_num_games; ++i) {
    blokus::GameRunner game(FLAGS_num_players);
    blokus::MctsOptions options{
      .c = 1.4,
      .num_iterations = FLAGS_num_mcts_iterations,
      .num_rollouts_per_iteration = FLAGS_num_mcts_rollouts,
      .num_threads = FLAGS_num_mcts_threads,
    };
    game.AddPlayer(absl::make_unique<blokus::MctsAI>(0, options));
    game.AddPlayer(absl::make_unique<blokus::MctsAI>(1, options));
    if (FLAGS_num_players == 4) {
      game.AddPlayer(absl::make_unique<blokus::MctsAI>(2, options));
      game.AddPlayer(absl::make_unique<blokus::MctsAI>(3, options));
    }

    if (FLAGS_print_board) {
      game.AddObserver(blokus::BoardPrintingObserver());
    }
    
    auto result = game.Play();
    for (int i = 0; i < FLAGS_num_players; ++i) {
      total_scores[i] += result.scores[i];
    }
  }
  absl::Time end = absl::Now();

  LOG(INFO) << "Played " << FLAGS_num_games << " games in " << (end - start);
  LOG(INFO) << "Average scores: ";
  for (int i = 0; i < FLAGS_num_players; ++i) {
    LOG(INFO) << i << ": " << (total_scores[i] / FLAGS_num_games);
  }
  
  return 0;
}
