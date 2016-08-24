#include "game.h"

#include <glog/logging.h>

namespace blokus {

Game::Game() {
}

Game::~Game() {
}

void Game::AddPlayer(Color color, std::unique_ptr<Player> player) {
  CHECK(players_.count(color) == 0) << "Already added a player for "
                                    << ColorToString(color);
  players_[color] = std::move(player);
}

void Game::Play() {
  static const std::vector<Color> kTurnOrder = {BLUE, YELLOW, RED, GREEN};
  CHECK(players_.size() == 4) << "Not enough players.";
  while (true) {
    for (Color color : kTurnOrder) {
      Move move;
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
      for (auto& observer : observers_) {
        observer(move, tile);
      }
      // TODO(piotrf): make board print an observer.
      board.Print(false);
    }
  }
}



}  // namespace blokus
