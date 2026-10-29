#include "game/game.h"

#include <array>

#include "absl/log/check.h"

namespace blokus {

namespace {

constexpr uint32_t kAllTiles = (1u << kNumTiles) - 1;

uint32_t ColorBit(Color color) { return 1u << color; }

}  // namespace

Game::Game(int num_players) : num_players_(num_players) {
  CHECK(num_players_ == 2 || num_players_ == 4);
  unplayed_tiles_ = {0, kAllTiles, kAllTiles, kAllTiles, kAllTiles};
  colors_with_moves_ =
      ColorBit(BLUE) | ColorBit(YELLOW) | ColorBit(RED) | ColorBit(GREEN);
}

bool Game::MakeMove(const Move& move) {
  if (move.color != current_color_) return false;
  if (move.tile < -1 || move.tile >= kNumTiles) return false;
  if (move.tile == -1) {
    colors_with_moves_ &= ~ColorBit(move.color);
  } else {
    // Once you pass, you can't keep playing.
    if (!(colors_with_moves_ & ColorBit(move.color))) {
      // TODO(piotrf): re-enable vlog once absl supports it
      //  VLOG(1) << ColorToString(move.color) << " already passed";
      return false;
    }
    
    // Ensure that this tile is still available.
    if (!(unplayed_tiles_[current_color_] & (1u << move.tile))) {
      // TODO(piotrf): re-enable vlog once absl supports it
      //  VLOG(1) << ColorToString(move.color) << " played an already played tile";
      return false;
    }
    
    // Try making the move.
    if (!board_.MakeMove(move)) {
      // TODO(piotrf): re-enable vlog once absl supports it
      //  VLOG(1) << ColorToString(move.color) << " doesn't fit the board";
      return false;
    }

    // If we succeeded, mark the tile as used.
    unplayed_tiles_[current_color_] &= ~(1u << move.tile);

    if (move.tile == 0) {
      played_one_last_ |= ColorBit(move.color);
    } else {
      played_one_last_ &= ~ColorBit(move.color);
    }
  }

  moves_.push_back(move);
  current_color_ = NextColor(current_color_);
  current_player_ = (current_player_ + 1) % num_players_;

  return true;
}

std::vector<Move> Game::PossibleMoves() const {
  // Once a color passes, it can only pass.
  if (!(colors_with_moves_ & ColorBit(current_color_))) return {};
  // The board leaves out the tiles this color has played.
  return board_.AllPossibleMoves(current_color_);
}

int Game::NumPossibleMoves() const {
  if (!(colors_with_moves_ & ColorBit(current_color_))) return 0;
  return board_.NumPossibleMoves(current_color_);
}

Move Game::PossibleMove(int index) const {
  CHECK_GE(index, 0);
  CHECK_LT(index, NumPossibleMoves());
  return board_.PossibleMove(current_color_, index);
}

std::array<int, kNumTiles> Game::NumPossibleMovesByTile() const {
  if (!(colors_with_moves_ & ColorBit(current_color_))) return {};
  return board_.NumPossibleMovesByTile(current_color_);
}

Move Game::PossibleMoveWithTile(int tile, int index) const {
  CHECK_GE(tile, 0);
  CHECK_LT(tile, kNumTiles);
  CHECK(colors_with_moves_ & ColorBit(current_color_));
  // The board checks `index`.
  return board_.PossibleMoveWithTile(current_color_, tile, index);
}

bool Game::Finished() const {
  return colors_with_moves_ == 0;
}

int Game::ColorScore(Color color) const {
  int score = 0;
  for (int tile = 0; tile < kNumTiles; ++tile) {
    if (unplayed_tiles_[color] & (1u << tile)) {
      score -= kTiles[tile].Size();
    }
  }
  if (score == 0) {
    score = (played_one_last_ & ColorBit(color)) ? 20 : 15;
  }
  return score;
}

GameResult Game::Result() const {
  // Score each color.
  std::array<int, 5> color_to_score = {};
  for (Color color : {BLUE, YELLOW, RED, GREEN}) {
    color_to_score[color] = ColorScore(color);
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
