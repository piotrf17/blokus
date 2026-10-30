#ifndef BLOKUS_ZERO_POLICY_AI_H_
#define BLOKUS_ZERO_POLICY_AI_H_

#include "game/board.h"
#include "game/game.h"
#include "game/player.h"
#include "zero/evaluator.h"

namespace blokus {

// An AI that plays the legal move its network's policy likes best, without
// searching. A first measure of how much a network has learned.
class PolicyAI : public Player {
 public:
  // `evaluator` must outlive the AI. Several AIs can share one evaluator.
  PolicyAI(int player_id, const Evaluator* evaluator)
      : Player(player_id), evaluator_(evaluator) {}

  Move SelectMove(const Game& game) override;

 private:
  const Evaluator* evaluator_;
};

}  // namespace blokus

#endif
