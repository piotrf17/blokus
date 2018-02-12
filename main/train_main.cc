#include "absl/memory/memory.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "ai/random.h"
#include "game/game.h"

DEFINE_int32(seed, -1, "Random number seed. If -1, use time.");
DEFINE_int32(num_games, 10, "Number of games to play.");

int main(int argc, char **argv) {
  // Initialize command line flags and logging.
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  if (FLAGS_seed != -1) {
    srand(FLAGS_seed);
  } else {
    srand(time(NULL));
  }

  std::map<blokus::Color, int> total_scores;

  absl::Time start = absl::Now();
  for (int i = 0; i < FLAGS_num_games; ++i) {
    blokus::Game game;
    game.AddPlayer(blokus::BLUE,
                   absl::make_unique<blokus::RandomAI>(blokus::BLUE));
    game.AddPlayer(blokus::YELLOW,
                   absl::make_unique<blokus::RandomAI>(blokus::YELLOW));
    game.AddPlayer(blokus::RED,
                   absl::make_unique<blokus::RandomAI>(blokus::RED));
    game.AddPlayer(blokus::GREEN,
                   absl::make_unique<blokus::RandomAI>(blokus::GREEN));

    auto result = game.Play();
    for (auto iter : result.scores) {
      total_scores[iter.first] += iter.second;
    }
  }
  absl::Time end = absl::Now();

  LOG(INFO) << "Played " << FLAGS_num_games << " games in " << (end - start);
  LOG(INFO) << "Average scores: ";
  for (auto iter : total_scores) {
    LOG(INFO) << blokus::ColorToString(iter.first) << ": "
              << (iter.second / FLAGS_num_games);
  }
  
  return 0;
}
