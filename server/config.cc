#include "server/config.h"

#include <algorithm>
#include <cmath>
#include <thread>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace blokus {
namespace {

using ::nlohmann::json;

// From analysis/mcts_iterations.md: about 880, 1780 and 2520 Elo.
constexpr struct {
  const char* name;
  int num_iterations;
  bool all_threads;
} kPresets[] = {
    {"low", 300, false},
    {"medium", 3000, false},
    {"high", 100000, true},
};

MctsOptions PresetOptions(int index) {
  MctsOptions options;
  options.num_iterations = kPresets[index].num_iterations;
  if (kPresets[index].all_threads) {
    options.num_threads =
        std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
  }
  return options;
}

json MctsOptionsToJson(const MctsOptions& options) {
  return {{"c", options.c},
          {"num_iterations", options.num_iterations},
          {"num_rollouts_per_iteration", options.num_rollouts_per_iteration},
          {"num_threads", options.num_threads},
          {"virtual_loss", options.virtual_loss}};
}

// Reads an integer field into `*value`, if present, checking its range.
absl::Status ReadInt(const json& object, const char* key, int min, int max,
                     int* value) {
  if (!object.contains(key)) return absl::OkStatus();
  const json& field = object[key];
  if (!field.is_number_integer() || field.get<int64_t>() < min ||
      field.get<int64_t>() > max) {
    return absl::InvalidArgumentError(absl::StrCat(
        "\"", key, "\" must be an integer from ", min, " to ", max));
  }
  *value = field.get<int>();
  return absl::OkStatus();
}

absl::StatusOr<MctsOptions> MctsOptionsFromJson(const json& mcts) {
  if (!mcts.is_object()) {
    return absl::InvalidArgumentError("\"mcts\" must be an object");
  }
  for (const auto& [key, value] : mcts.items()) {
    if (key != "c" && key != "num_iterations" &&
        key != "num_rollouts_per_iteration" && key != "num_threads" &&
        key != "virtual_loss") {
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown MCTS option \"", key, "\""));
    }
  }
  MctsOptions options;
  if (mcts.contains("c")) {
    const json& c = mcts["c"];
    if (!c.is_number() || !std::isfinite(c.get<double>()) ||
        c.get<double>() < 0 || c.get<double>() > 100) {
      return absl::InvalidArgumentError(
          "\"c\" must be a number from 0 to 100");
    }
    options.c = c.get<double>();
  }
  if (absl::Status status = ReadInt(mcts, "num_iterations", 1, 10'000'000,
                                    &options.num_iterations);
      !status.ok()) {
    return status;
  }
  if (absl::Status status = ReadInt(mcts, "num_rollouts_per_iteration", 1,
                                    100, &options.num_rollouts_per_iteration);
      !status.ok()) {
    return status;
  }
  if (absl::Status status =
          ReadInt(mcts, "num_threads", 1, 256, &options.num_threads);
      !status.ok()) {
    return status;
  }
  if (absl::Status status =
          ReadInt(mcts, "virtual_loss", 0, 100, &options.virtual_loss);
      !status.ok()) {
    return status;
  }
  return options;
}

absl::StatusOr<SeatConfig> SeatConfigFromJson(const json& seat_json) {
  if (!seat_json.is_object() || !seat_json.contains("type") ||
      !seat_json["type"].is_string()) {
    return absl::InvalidArgumentError(
        "Each seat must be an object with a \"type\"");
  }
  SeatConfig seat;
  const std::string type = seat_json["type"].get<std::string>();
  if (type == "human") return seat;
  if (type != "ai") {
    return absl::InvalidArgumentError(
        "A seat's \"type\" must be \"human\" or \"ai\"");
  }
  seat.human = false;

  if (seat_json.contains("preset") && !seat_json["preset"].is_string()) {
    return absl::InvalidArgumentError("\"preset\" must be a string");
  }
  seat.preset = seat_json.value("preset", "custom");
  if (seat_json.contains("mcts")) {
    absl::StatusOr<MctsOptions> options =
        MctsOptionsFromJson(seat_json["mcts"]);
    if (!options.ok()) return options.status();
    seat.mcts = *options;
    return seat;
  }
  for (int i = 0; i < static_cast<int>(std::size(kPresets)); ++i) {
    if (seat.preset == kPresets[i].name) {
      seat.mcts = PresetOptions(i);
      return seat;
    }
  }
  return absl::InvalidArgumentError(
      absl::StrCat("An AI seat needs \"mcts\" options or a preset: \"",
                   seat.preset, "\" isn't one"));
}

}  // namespace

json PresetsToJson() {
  json presets = json::object();
  for (int i = 0; i < static_cast<int>(std::size(kPresets)); ++i) {
    presets[kPresets[i].name] = MctsOptionsToJson(PresetOptions(i));
  }
  return presets;
}

absl::StatusOr<GameConfig> GameConfigFromJson(const json& config_json) {
  if (!config_json.is_object()) {
    return absl::InvalidArgumentError("The config must be an object");
  }
  GameConfig config;
  if (!config_json.contains("num_players") ||
      (config_json["num_players"] != 2 && config_json["num_players"] != 4)) {
    return absl::InvalidArgumentError("\"num_players\" must be 2 or 4");
  }
  config.num_players = config_json["num_players"].get<int>();
  if (!config_json.contains("seats") || !config_json["seats"].is_array() ||
      config_json["seats"].size() != config.num_players) {
    return absl::InvalidArgumentError(
        "\"seats\" must have one entry per player");
  }
  for (const json& seat_json : config_json["seats"]) {
    absl::StatusOr<SeatConfig> seat = SeatConfigFromJson(seat_json);
    if (!seat.ok()) return seat.status();
    config.seats.push_back(*std::move(seat));
  }
  return config;
}

json GameConfigToJson(const GameConfig& config) {
  json seats = json::array();
  for (const SeatConfig& seat : config.seats) {
    if (seat.human) {
      seats.push_back({{"type", "human"}});
    } else {
      seats.push_back({{"type", "ai"},
                       {"preset", seat.preset},
                       {"mcts", MctsOptionsToJson(seat.mcts)}});
    }
  }
  return {{"num_players", config.num_players}, {"seats", std::move(seats)}};
}

}  // namespace blokus
