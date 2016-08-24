#ifndef BLOKUS_GAME_H_
#define BLOKUS_GAME_H_

#include <map>
#include <memory>
#include <vector>

#include "board.h"
#include "player.h"

namespace blokus {

// Class for managing a game of Blokus. Currently only handles 4 player games.
class Game {
 public:
  Game();
  ~Game();

  void AddPlayer(Color color, std::unique_ptr<Player> player);

  typedef std::function<void(const Move& move, int)> ObserverFunc;
  void AddObserver(ObserverFunc observer) {
    observers_.push_back(observer);
  }

  void Play();
  
 private:
  std::map<Color, std::unique_ptr<Player>> players_;
  std::vector<ObserverFunc> observers_;
  Board board;
};

}  // namespace blokus

#endif
