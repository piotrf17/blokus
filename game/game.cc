#include "game/game.h"

#include <set>

#include <glog/logging.h>

namespace blokus {

Game::Game(int num_players) {
  CHECK(num_players == 2 || num_players == 4);
  num_players = num_players;
  player_tiles_[BLUE] = std::vector<bool>(kNumTiles, true);
  player_tiles_[YELLOW] = std::vector<bool>(kNumTiles, true);
  player_tiles_[RED] = std::vector<bool>(kNumTiles, true);
  player_tiles_[GREEN] = std::vector<bool>(kNumTiles, true);
  players_with_moves_ = {BLUE, YELLOW, RED, GREEN};
}

bool Game::MakeMove(const Move& move) {
  if (move.color != current_color_) return false;
  if (move.tile == -1) {
    players_with_moves_.erase(move.color);
  } else {
    // Once you pass, you can't keep playing.
    if (players_with_moves_.count(move.color) == 0) {
      VLOG(1) << ColorToString(move.color) << " already passed";
      return false;
    }
    
    // Ensure that this tile is still available.
    if (!player_tiles_[current_color_][move.tile]) {
      VLOG(1) << ColorToString(move.color) << " played an already played tile";
      return false;
    }
    
    // Try making the move.
    if (!board_.MakeMove(move)) {
      VLOG(1) << ColorToString(move.color) << " doesn't fit the board";
      return false;
    }

    // If we succeeded, mark the tile as used.
    player_tiles_[current_color_][move.tile] = false;

    if (move.tile == 0) {
      played_one_last_.insert(move.color);
    } else {
      played_one_last_.erase(move.color);
    }
  }

  moves_.push_back(move);
  current_color_ = NextColor(current_color_);
  current_player_ = (current_player_ + 1) % num_players_;

  return true;
}

std::vector<Move> Game::PossibleMoves() const {
  std::vector<Move> moves;
  for (int tile = 0; tile < kNumTiles; ++tile) {
    if (!player_tiles_.at(current_color_)[tile]) continue;
    std::vector<Move> tile_moves = board_.PossibleMoves(
        kTiles[tile], current_color_);
    moves.insert(moves.end(), tile_moves.begin(), tile_moves.end());
  }
  return moves;
}

bool Game::Finished() const {
  return players_with_moves_.empty();
}

GameResult Game::Result() const {
  // Score each color.
  std::map<Color, int> color_to_score;
  for (Color color : {BLUE, YELLOW, RED, GREEN}) {
    // Compute the score.
    int score = 0;
    for (int tile = 0; tile < 21; ++tile) {
      if (player_tiles_.at(color)[tile]) {
        score -= kTiles[tile].Size();
      }
    }
    if (score == 0) {
      if (played_one_last_.count(color)) score = 20;
      else score = 15;
    }
    color_to_score[color] = score;
  }

  // Convert color scores to player scores.
  GameResult result;
  
  result.scores.resize(num_players_);
  if (num_players_ == 2) {
    result.scores[0] = color_to_score[BLUE] + color_to_score[RED];
    result.scores[1] = color_to_score[YELLOW] + color_to_score[GREEN];
  } else {
    result.scores[0] = color_to_score[BLUE];
    result.scores[1] = color_to_score[YELLOW];
    result.scores[2] = color_to_score[RED];
    result.scores[3] = color_to_score[GREEN];
  }
  
  // Compute the winner.
  int max_score = -10000;
  for (int i = 0; i < result.scores.size(); ++i) {
    if (result.scores[i] > max_score) {
      result.winner_id = i;
      max_score = result.scores[i];
    }
  }
  
  return result;
}

}  // namespace blokus
