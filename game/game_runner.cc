#include "game/game_runner.h"

#include <set>

#include "absl/log/check.h"

#include "game/game.h"

namespace blokus {

void GameRunner::AddPlayer(std::unique_ptr<Player> player) {
  CHECK_LT(players_.size(), num_players_)
      << "Added too many players, game is for " << num_players_;
  CHECK_EQ(player->player_id(), players_.size())
      << "expected player with id " << players_.size();
  players_.push_back(std::move(player));
}

GameResult GameRunner::Play() {
  CHECK_EQ(players_.size(), num_players_) << "Not enough players.";

  Game game(num_players_);
  while (!game.Finished()) {
    int current_player = game.current_player();
    Move move = players_[current_player]->SelectMove(game);
    // TODO(piotrf): re-enable once absl supports vlog
    //  VLOG(1) << move.DebugString();
    CHECK(game.MakeMove(move))
        << ColorToString(game.current_color()) << " (player " << current_player
        << ") wants to play an invalid move: " << move.DebugString();
    for (auto& observer : observers_) {
      observer(game, move);
    }
  }

  return game.Result();
}

ObserverFunc BoardPrintingObserver(bool include_debug) {
  return [include_debug](const Game& game, const Move& move) {
    printf("%s\n", move.DebugString().c_str());
    game.board().Print(include_debug);
  };
}


}  // namespace blokus
