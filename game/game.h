#ifndef BLOKUS_GAME_GAME_H_
#define BLOKUS_GAME_GAME_H_

#include <array>
#include <cstdint>
#include <vector>

#include "game/board.h"

namespace blokus {

// The final result of the game.
struct GameResult {
  // The player id of the winning player.
  // Note: this is just the first player to achieve the maximum score, so it
  // doesn't really deal with ties.
  int winner_id;

  // The scores for all players.
  std::vector<int> scores;
};

// Manages game state beyond the board, e.g. current player, tiles held, etc.
class Game {
 public:
  // Create a game for `num_players` players, which must be either 2 or 4.
  explicit Game(int num_players);
  
  // Make a move for the current player color.
  // If the move is valid, returns true and advances to the next player.
  // Otherwise returns false and stays in the same state.
  bool MakeMove(const Move& move);

  // Returns a list of possible moves that the current player color can play.
  // Passing is always allowed and is not included. Empty if the current color
  // has already passed.
  std::vector<Move> PossibleMoves() const;

  // Returns PossibleMoves().size(), without building the moves.
  int NumPossibleMoves() const;

  // Returns PossibleMoves()[index], without building all the moves.
  Move PossibleMove(int index) const;

  // Returns true if the game is finished.
  bool Finished() const;

  // Returns the final result of the game.
  // Must have Finished() == true.
  GameResult Result() const;

  int num_players() const { return num_players_; }
  int current_player() const { return current_player_; }
  Color current_color() const { return current_color_; }
  const Board& board() const { return board_; }
  const std::vector<Move>& moves() const { return moves_; }
  
 private:
  int num_players_;
  // The current player id.
  int current_player_ = 0;
  // The color of the current player, must match the next call to Move().
  Color current_color_ = BLUE;

  // The game board.
  Board board_;
  // List of all played moves.
  std::vector<Move> moves_;
  // Indexed by color, a bit per tile. A set bit (1 << tile) means the tile
  // hasn't been played.
  std::array<uint32_t, 5> unplayed_tiles_;
  // A bit (1 << color) for each color that has not yet passed.
  uint32_t colors_with_moves_;
  // A bit (1 << color) for each color whose last tile played was the '1' tile.
  uint32_t played_one_last_ = 0;
};

}  // namespace blokus

#endif
