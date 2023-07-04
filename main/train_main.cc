#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/initialize.h"
#include "absl/memory/memory.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

#include "ai/mcts.h"
#include "ai/random.h"
#include "game/game_runner.h"

ABSL_FLAG(int, seed, -1, "Random number seed. If -1, use time.");
ABSL_FLAG(int, num_games, 10, "Number of games to play.");
ABSL_FLAG(bool, print_board, false, "Print the board during play.");

ABSL_FLAG(int, num_players, 2, "Number of players, 2 or 4.");
ABSL_FLAG(int, num_mcts_iterations, 10000,
          "Number of MCTS iterations to run per move.");
ABSL_FLAG(int, num_mcts_rollouts, 1,
          "Number of MCTS rollouts per iterations.");
ABSL_FLAG(int, num_mcts_threads, 1, "Number of MCTS threads.");

int main(int argc, char **argv) {
  // Initialize command line flags and logging.
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();

  const int num_players = absl::GetFlag(FLAGS_num_players);
  CHECK(num_players == 2 || num_players == 4);
  const int num_games = absl::GetFlag(FLAGS_num_games);

  if (absl::GetFlag(FLAGS_seed) != -1) {
    srand(absl::GetFlag(FLAGS_seed));
  } else {
    srand(time(NULL));
  }

  std::vector<int> total_scores(num_players, 0);

  absl::Time start = absl::Now();
  for (int i = 0; i < num_games; ++i) {
    blokus::GameRunner game(num_players);
    blokus::MctsOptions options{
      .c = 1.4,
      .num_iterations = absl::GetFlag(FLAGS_num_mcts_iterations),
      .num_rollouts_per_iteration = absl::GetFlag(FLAGS_num_mcts_rollouts),
      .num_threads = absl::GetFlag(FLAGS_num_mcts_threads),
    };
    game.AddPlayer(absl::make_unique<blokus::MctsAI>(0, options));
    game.AddPlayer(absl::make_unique<blokus::MctsAI>(1, options));
    if (num_players == 4) {
      game.AddPlayer(absl::make_unique<blokus::MctsAI>(2, options));
      game.AddPlayer(absl::make_unique<blokus::MctsAI>(3, options));
    }

    if (absl::GetFlag(FLAGS_print_board)) {
      game.AddObserver(blokus::BoardPrintingObserver());
    }
    
    auto result = game.Play();
    for (int i = 0; i < num_players; ++i) {
      total_scores[i] += result.scores[i];
    }
  }
  absl::Time end = absl::Now();

  LOG(INFO) << "Played " << num_games << " games in " << (end - start);
  LOG(INFO) << "Average scores: ";
  for (int i = 0; i < num_players; ++i) {
    LOG(INFO) << i << ": " << (total_scores[i] / num_games);
  }
  
  return 0;
}
