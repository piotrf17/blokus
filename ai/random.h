#ifndef BLOKUS_AI_RANDOM_H
#define BLOKUS_AI_RANDOM_H

#include <cstdint>
#include <optional>
#include <random>

#include "game/player.h"

namespace blokus {

// An AI that just plays a random move from the set of possible moves.
class RandomAI : public Player {
 public:
  // If `seed` is unset, seeds from std::random_device.
  explicit RandomAI(int player_id, std::optional<uint64_t> seed = std::nullopt)
      : Player(player_id),
        rng_(seed.has_value() ? *seed : std::random_device()()) {}

  Move SelectMove(const Game& game) override;

 private:
  std::mt19937_64 rng_;
};

}  // namespace blokus

#endif
