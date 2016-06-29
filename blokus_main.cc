#include <gflags/gflags.h>
#include <glog/logging.h>
#include <memory>
#include <set>

#include "game.h"

namespace blokus {

class RandomAI : public Player {
 public:
  RandomAI(Color color) : Player(color) {}

  bool SelectMove(const Board& board, Move* move, int* chosen_tile) {
    for (int tile = 20; tile >= 0; tile --) {
      if (played_tiles.count(tile) > 0) continue;
      auto moves = board.PossibleMoves(kTiles[tile], color());
      if (moves.size() > 0) {
        *move = moves[rand() % moves.size()];
        *chosen_tile = tile;
        played_tiles.insert(tile);
        return true;
      }
    }
    return false;
  }

 private:
  std::set<int> played_tiles;
};

}  // namespace blokus


int main(int argc, char **argv) {
  // Initialize command line flags and logging.
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  blokus::Game game;
  game.AddPlayer(blokus::BLUE,
                 std::make_unique<blokus::RandomAI>(blokus::BLUE));
  game.AddPlayer(blokus::YELLOW,
                 std::make_unique<blokus::RandomAI>(blokus::YELLOW));
  game.AddPlayer(blokus::RED,
                 std::make_unique<blokus::RandomAI>(blokus::RED));
  game.AddPlayer(blokus::GREEN,
                 std::make_unique<blokus::RandomAI>(blokus::GREEN));

  game.Play();
  
  return 0;
}
