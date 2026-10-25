#include "ai/random.h"

namespace blokus {

Move RandomAI::SelectMove(const Game& game) {
  const std::vector<Move> moves = game.PossibleMoves();
  if (!moves.empty()) {
    std::uniform_int_distribution<int> pick(0, moves.size() - 1);
    return moves[pick(rng_)];
  }
  return Move::EmptyMove(game.current_color());
}

}  // namespace blokus
