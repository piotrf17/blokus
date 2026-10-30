#include "zero/zero_ai.h"

namespace blokus {

Move ZeroAI::SelectMove(const Game& game) {
  return Search(game, *evaluator_, options_, rng_()).move;
}

}  // namespace blokus
