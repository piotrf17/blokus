#ifndef BLOKUS_SERVER_CONFIG_H_
#define BLOKUS_SERVER_CONFIG_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"

#include "ai/mcts.h"

namespace blokus {

// Who plays a seat. A seat is a player id: in 2-player games each seat plays
// two colors.
struct SeatConfig {
  bool human = true;
  // For an AI seat: the preset it was made from ("low", "medium", "high"), or
  // "custom".
  std::string preset;
  // For an AI seat: the search options, already resolved from the preset.
  MctsOptions mcts;
};

struct GameConfig {
  int num_players = 2;
  // One per player id.
  std::vector<SeatConfig> seats;
};

// The AI presets, as {"low": {mcts options}, ...} in increasing strength.
// "high" uses one thread per hardware thread.
nlohmann::json PresetsToJson();

// Parses a new game's config:
//   {"num_players": 2,
//    "seats": [{"type": "human"},
//              {"type": "ai", "preset": "medium"},
//              {"type": "ai", "mcts": {"num_iterations": 5000, ...}}]}
// A custom "mcts" can set any of "c", "num_iterations",
// "num_rollouts_per_iteration", "num_threads" and "virtual_loss"; the others
// keep their MctsOptions defaults. Returns InvalidArgument for anything
// malformed or out of range.
absl::StatusOr<GameConfig> GameConfigFromJson(const nlohmann::json& config);

// The inverse of GameConfigFromJson. AI seats always include their resolved
// "mcts" options, along with their "preset".
nlohmann::json GameConfigToJson(const GameConfig& config);

}  // namespace blokus

#endif  // BLOKUS_SERVER_CONFIG_H_
