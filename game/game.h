#ifndef BLOKUS_GAME_GAME_H_
#define BLOKUS_GAME_GAME_H_

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "game/board.h"
#include "game/player.h"

namespace blokus {

// Interface for observers. After every move, the observer will be called
// with the latest move as well as the current board state.
// TODO(piotrf): how to represent no move?
typedef std::function<void(const Board& board, const Move& move,
                           int tile)> ObserverFunc;

struct GameResult {
  std::map<Color, int> scores;
};

// Class for managing a game of Blokus. Currently only handles 4 player games.
class Game {
 public:
  Game();
  ~Game();

  // Add a player of the given color to the game.
  void AddPlayer(Color color, std::unique_ptr<Player> player);

  // Add an observer, will be called after every move.
  void AddObserver(ObserverFunc observer) {
    observers_.push_back(observer);
  }

  // Play the game until completion.
  GameResult Play();
  
 private:
  std::map<Color, std::unique_ptr<Player>> players_;
  std::vector<ObserverFunc> observers_;
  Board board_;
};

// Observer that prints the board to stdout.
ObserverFunc BoardPrintingObserver(bool include_debug = false);

}  // namespace blokus

#endif
