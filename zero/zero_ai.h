#ifndef BLOKUS_ZERO_ZERO_AI_H_
#define BLOKUS_ZERO_ZERO_AI_H_

#include <cstdint>
#include <random>

#include "game/board.h"
#include "game/game.h"
#include "game/player.h"
#include "zero/evaluator.h"
#include "zero/search.h"

namespace blokus {

// An AI that plays the move a Gumbel search (see zero/search.h) with its
// network picks. Only for 2-player games.
class ZeroAI : public Player {
 public:
  // `evaluator` must outlive the AI. Several AIs can share one evaluator.
  ZeroAI(int player_id, const Evaluator* evaluator,
         const SearchOptions& options, uint64_t seed)
      : Player(player_id), evaluator_(evaluator), options_(options),
        rng_(seed) {}

  Move SelectMove(const Game& game) override;

 private:
  const Evaluator* evaluator_;
  SearchOptions options_;
  std::mt19937_64 rng_;
};

}  // namespace blokus

#endif
