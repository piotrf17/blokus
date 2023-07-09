#ifndef BLOKUS_GAME_LOGGER_H_
#define BLOKUS_GAME_LOGGER_H_

#include <fstream>

#include "game/game_log.pb.h"
#include "game/game_runner.h"

namespace blokus {

class GameLogger {
 public:
  // Publishes a log of games played to `output_path`.
  // The file is written when GameLogger is destructed.
  explicit GameLogger(std::string_view output_path);
  ~GameLogger();
  
  // Start a new game record, and return an observer function that can
  // be added to watch game moves.
  ObserverFunc NewGame();

 private:
  void AddMove(const Game& game, const Move& move);

  std::ofstream outfile_;

  GameLog* current_;
  GamesLog all_games_;
};

}  // namespace blokus

#endif
