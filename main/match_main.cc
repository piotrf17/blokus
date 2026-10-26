// Plays 2-player games between two MCTS configurations, A and B, and reports
// A's results. The configurations swap seats every game, so each plays first
// (BLUE and RED) in half of the games.
//
//   bazel run -c opt //main:match -- --num_games=100 --a_threads=4
#include <cmath>
#include <memory>
#include <random>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

#include "ai/mcts.h"
#include "game/game.h"
#include "game/game_runner.h"
#include "game/player.h"

ABSL_FLAG(int, num_games, 20, "Number of games to play. Should be even.");
ABSL_FLAG(int, seed, 0, "Random number seed for the whole match.");

ABSL_FLAG(int, a_iterations, 1000, "MCTS iterations per move for A.");
ABSL_FLAG(int, a_threads, 1, "MCTS threads for A.");
ABSL_FLAG(double, a_c, blokus::MctsOptions().c,
          "UCB1 exploration constant for A.");
ABSL_FLAG(int, a_virtual_loss, blokus::MctsOptions().virtual_loss,
          "MCTS virtual loss for A.");

ABSL_FLAG(int, b_iterations, 1000, "MCTS iterations per move for B.");
ABSL_FLAG(int, b_threads, 1, "MCTS threads for B.");
ABSL_FLAG(double, b_c, blokus::MctsOptions().c,
          "UCB1 exploration constant for B.");
ABSL_FLAG(int, b_virtual_loss, blokus::MctsOptions().virtual_loss,
          "MCTS virtual loss for B.");

namespace blokus {
namespace {

// Wraps a player, adding the time it spends in SelectMove to `*total`.
class TimedPlayer : public Player {
 public:
  TimedPlayer(std::unique_ptr<Player> player, absl::Duration* total)
      : Player(player->player_id()), player_(std::move(player)), total_(total) {}

  Move SelectMove(const Game& game) override {
    const absl::Time start = absl::Now();
    Move move = player_->SelectMove(game);
    *total_ += absl::Now() - start;
    return move;
  }

 private:
  std::unique_ptr<Player> player_;
  absl::Duration* total_;
};

struct Config {
  MctsOptions options;
  absl::Duration time;
};

int Main() {
  const int num_games = absl::GetFlag(FLAGS_num_games);
  CHECK_GT(num_games, 0);
  Config a{.options = {.c = absl::GetFlag(FLAGS_a_c),
                       .num_iterations = absl::GetFlag(FLAGS_a_iterations),
                       .num_threads = absl::GetFlag(FLAGS_a_threads),
                       .virtual_loss = absl::GetFlag(FLAGS_a_virtual_loss)}};
  Config b{.options = {.c = absl::GetFlag(FLAGS_b_c),
                       .num_iterations = absl::GetFlag(FLAGS_b_iterations),
                       .num_threads = absl::GetFlag(FLAGS_b_threads),
                       .virtual_loss = absl::GetFlag(FLAGS_b_virtual_loss)}};

  std::mt19937_64 seeder(absl::GetFlag(FLAGS_seed));
  // A's points: 1 per win, 0.5 per tie.
  double points = 0;
  double sum_squared_points = 0;
  int wins = 0;
  int ties = 0;
  int64_t margin = 0;
  for (int i = 0; i < num_games; ++i) {
    // A is player 0 in even games, and player 1 in odd games.
    const int a_id = i % 2;
    Config* configs[2] = {a_id == 0 ? &a : &b, a_id == 0 ? &b : &a};
    GameRunner runner(2);
    for (int id = 0; id < 2; ++id) {
      MctsOptions options = configs[id]->options;
      options.seed = seeder();
      runner.AddPlayer(std::make_unique<TimedPlayer>(
          std::make_unique<MctsAI>(id, options), &configs[id]->time));
    }
    const GameResult result = runner.Play();

    const int a_score = result.scores[a_id];
    const int b_score = result.scores[1 - a_id];
    const double game_points =
        a_score > b_score ? 1.0 : (a_score == b_score ? 0.5 : 0.0);
    points += game_points;
    sum_squared_points += game_points * game_points;
    wins += a_score > b_score;
    ties += a_score == b_score;
    margin += a_score - b_score;
    LOG(INFO) << absl::StrFormat("Game %d: A (player %d) %d, B %d", i, a_id,
                                 a_score, b_score);
  }

  // Normal approximation for the 95% confidence interval of A's mean points.
  const double mean = points / num_games;
  const double variance = sum_squared_points / num_games - mean * mean;
  const double half_width = 1.96 * std::sqrt(variance / num_games);
  absl::PrintF("A: %d wins, %d ties, %d losses in %d games\n", wins, ties,
               num_games - wins - ties, num_games);
  absl::PrintF("A score: %.3f +/- %.3f (95%% CI), mean margin %+.1f\n", mean,
               half_width, static_cast<double>(margin) / num_games);
  absl::PrintF("Time in SelectMove: A %s, B %s\n", absl::FormatDuration(a.time),
               absl::FormatDuration(b.time));
  return 0;
}

}  // namespace
}  // namespace blokus

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  return blokus::Main();
}
