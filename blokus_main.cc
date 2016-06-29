#include <gflags/gflags.h>
#include <glog/logging.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "board.h"
#include "tile.h"

namespace blokus {

class Player {
 public:
  explicit Player(Color color) : color_(color) {}
  
  virtual bool SelectMove(const Board& board, Move* move, int* chosen_tile) = 0;

  Color color() const { return color_; }
  
 private:
  Color color_;
};

class Game {
 public:
  Game() {}
  ~Game() {}

  void AddPlayer(Color color, std::unique_ptr<Player> player) {
    CHECK(players_.count(color) == 0) << "Already added a player for "
                                      << ColorToString(color);
    players_[color] = std::move(player);
  }

  void Play() {
    static const std::vector<Color> kTurnOrder = {BLUE, YELLOW, RED, GREEN};
    while (true) {
      for (Color color : kTurnOrder) {
        blokus::Move move;
        int tile;
        if (players_[color]->SelectMove(board, &move, &tile)) {
          if (!board.Place(blokus::kTiles[tile], color, move)) {
            LOG(FATAL) << ColorToString(color)
                       << " wants to play an invalid move: "
                       << move.DebugString();
          }
        } else {
          printf("%s is out of moves\n", blokus::ColorToString(color).c_str());
        }
        board.Print(false);
        getchar();
      }
    }
  }
  
 private:
  std::map<Color, std::unique_ptr<Player>> players_;
  Board board;
};

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
