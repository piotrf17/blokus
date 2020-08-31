#ifndef BLOKUS_GAME_GAME_RUNNER_H_
#define BLOKUS_GAME_GAME_RUNNER_H_

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "game/player.h"

namespace blokus {

// Interface for observers. After every move, the observer will be called
// with the latest move as well as the current board state.
typedef std::function<void(const Board& board, const Move& move)> ObserverFunc;

// Class for managing a game of Blokus.
class GameRunner {
 public:
  // `num_players` must be either 2 or 4.
  explicit GameRunner(int num_players) : num_players_(num_players) {}

  // Add a player to the game.
  //
  // Players are added in order. For a two player game, this is:
  //   0: BLUE, RED
  //   1: YELLOW, GREEN
  // For a four player game, this is:
  //   0: BLUE
  //   1: YELLOW
  //   2: RED
  //   3: GREEN
  void AddPlayer(std::unique_ptr<Player> player);

  // Add an observer, will be called after every move.
  void AddObserver(ObserverFunc observer) {
    observers_.push_back(observer);
  }

  // Play the game until completion.
  GameResult Play();
  
 private:
  int num_players_;
  std::vector<std::unique_ptr<Player>> players_;
  std::vector<ObserverFunc> observers_;
};

// Observer that prints the board to stdout.
ObserverFunc BoardPrintingObserver(bool include_debug = false);

}  // namespace blokus

#endif
