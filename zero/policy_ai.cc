#include "zero/policy_ai.h"

#include <algorithm>
#include <vector>

namespace blokus {

Move PolicyAI::SelectMove(const Game& game) {
  if (game.NumPossibleMoves() == 0) {
    return Move::EmptyMove(game.current_color());
  }
  const Game* games[] = {&game};
  const Evaluation evaluation = evaluator_->Evaluate(games)[0];
  const auto best = std::max_element(evaluation.priors.begin(),
                                     evaluation.priors.end());
  return evaluation.moves[best - evaluation.priors.begin()];
}

}  // namespace blokus
