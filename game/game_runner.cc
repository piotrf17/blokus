#include "game/game_runner.h"

#include <glog/logging.h>
#include <set>

#include "game/game.h"

namespace blokus {

void GameRunner::AddPlayer(Color color, std::unique_ptr<Player> player) {
  CHECK(players_.count(color) == 0) << "Already added a player for "
                                    << ColorToString(color);
  players_[color] = std::move(player);
}

GameResult GameRunner::Play() {
  CHECK(players_.size() == 4) << "Not enough players.";

  Game game;
  while (!game.Finished()) {
    Move move = players_[game.current_color()]->SelectMove(game);
    CHECK(game.MakeMove(move)) << ColorToString(game.current_color())
                               << " wants to play an invalid move: "
                               << move.DebugString();
    for (auto& observer : observers_) {
      observer(game.board(), move);
    }
  }

  return game.Result();
}

ObserverFunc BoardPrintingObserver(bool include_debug) {
  return [include_debug](const Board& board, const Move& move) {
    printf("%s\n", move.DebugString().c_str());
    board.Print(include_debug);
  };
}


}  // namespace blokus
