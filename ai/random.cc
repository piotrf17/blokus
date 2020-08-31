#include "ai/random.h"

namespace blokus {

Move RandomAI::SelectMove(const Game& game) {
  std::vector<Move> moves = game.PossibleMoves();
  if (moves.size() > 0) {
    const Move& move = moves[rand() % moves.size()];
    played_tiles_.insert(move.tile);
    return move;
  }
  return Move::EmptyMove(game.current_color());
}

}  // namespace blokus
