#include "absl/memory/memory.h"
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "ai/random.h"
#include "game/game.h"

int main(int argc, char **argv) {
  // Initialize command line flags and logging.
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  srand(time(NULL)); // TODO(piotrf): set via flag.

  blokus::Game game;
  game.AddPlayer(blokus::BLUE,
                 absl::make_unique<blokus::RandomAI>(blokus::BLUE));
  game.AddPlayer(blokus::YELLOW,
                 absl::make_unique<blokus::RandomAI>(blokus::YELLOW));
  game.AddPlayer(blokus::RED,
                 absl::make_unique<blokus::RandomAI>(blokus::RED));
  game.AddPlayer(blokus::GREEN,
                 absl::make_unique<blokus::RandomAI>(blokus::GREEN));

  game.Play();
  
  return 0;
}
