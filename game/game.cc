#include "game/game.h"

#include <set>

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

GameResult Game::Play() {
  static const std::vector<Color> kTurnOrder = {BLUE, YELLOW, RED, GREEN};
  CHECK(players_.size() == 4) << "Not enough players.";
  
  std::set<Color> players_with_moves = {BLUE, YELLOW, RED, GREEN};
  std::map<Color, std::set<int>> played_tiles;
  std::map<Color, bool> played_one_last;
  
  while (!players_with_moves.empty()) {
    for (Color color : kTurnOrder) {
      Move move;
      int tile;
      if (players_[color]->SelectMove(board_, &move, &tile)) {
        if (!board_.Place(kTiles[tile], color, move)) {
          LOG(FATAL) << ColorToString(color)
                     << " wants to play an invalid move: "
                     << move.DebugString();
        }
        played_tiles[color].insert(tile);
        if (tile == 0) {
          played_one_last[color] = true;
        } else {
          played_one_last[color] = false;
        }
      } else {
        players_with_moves.erase(color);
      }
      for (auto& observer : observers_) {
        observer(board_, move, tile);
      }
    }
  }

  GameResult result;
  
  // Compute the scores.
  for (Color color : kTurnOrder) {
    int score = 0;
    for (int tile = 0; tile < 21; ++tile) {
      if (!played_tiles[color].count(tile)) {
        score -= kTiles[tile].Size();
      }
    }
    if (score == 0) {
      if (played_one_last[color]) score = 20;
      else score = 15;
    }
    result.scores[color] = score;
  }

  return result;
}

ObserverFunc BoardPrintingObserver(bool include_debug) {
  return [include_debug](const Board& board, const Move& move, int tile) {
    // TODO(piotrf): need to pass color to this observer.
    //    printf("%s played at %s\n", ColorToString(color).c_str(),
    //           move.DebugString().c_str());
    board.Print(include_debug);
  };
}


}  // namespace blokus
