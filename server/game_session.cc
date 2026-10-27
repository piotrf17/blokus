#include "server/game_session.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"

#include "ai/mcts.h"
#include "server/json.h"

namespace blokus {
namespace {

using ::nlohmann::json;

std::string TimeToString(absl::Time time) {
  return absl::FormatTime(absl::RFC3339_full, time, absl::UTCTimeZone());
}

}  // namespace

AiFactory MctsAiFactory() {
  return [](int player_id, const SeatConfig& seat) {
    return std::make_unique<MctsAI>(player_id, seat.mcts);
  };
}

GameSession::GameSession(std::string id, GameConfig config,
                         AiFactory ai_factory, std::string save_path,
                         ChangeCallback on_change)
    : id_(std::move(id)),
      config_(std::move(config)),
      ai_factory_(std::move(ai_factory)),
      save_path_(std::move(save_path)),
      on_change_(std::move(on_change)),
      game_(config_.num_players),
      updated_(absl::Now()) {}

absl::StatusOr<std::unique_ptr<GameSession>> GameSession::Create(
    std::string id, GameConfig config, const std::vector<Move>& moves,
    AiFactory ai_factory, std::string save_path, ChangeCallback on_change) {
  if (config.num_players != 2 && config.num_players != 4) {
    return absl::InvalidArgumentError("A game needs 2 or 4 players");
  }
  if (config.seats.size() != config.num_players) {
    return absl::InvalidArgumentError("A game needs one seat per player");
  }
  std::unique_ptr<GameSession> session(
      new GameSession(std::move(id), std::move(config), std::move(ai_factory),
                      std::move(save_path), std::move(on_change)));
  for (const Move& move : moves) {
    if (!session->game_.MakeMove(move)) {
      return absl::InvalidArgumentError(
          absl::StrCat("Illegal move: ", move.DebugString()));
    }
  }
  session->ResetAis();
  session->PlayForcedPasses();
  session->Save();
  return session;
}

absl::StatusOr<std::unique_ptr<GameSession>> GameSession::Load(
    const json& saved, AiFactory ai_factory, std::string save_path,
    ChangeCallback on_change) {
  if (!saved.is_object() || !saved.contains("id") ||
      !saved["id"].is_string() || !saved.contains("config") ||
      !saved.contains("moves") || !saved["moves"].is_array() ||
      !saved.contains("updated") || !saved["updated"].is_string()) {
    return absl::InvalidArgumentError("Not a saved game");
  }
  absl::StatusOr<GameConfig> config = GameConfigFromJson(saved["config"]);
  if (!config.ok()) return config.status();
  // Replay the moves on a scratch game, to parse each one in its position.
  Game game(config->num_players);
  std::vector<Move> moves;
  for (const json& move_json : saved["moves"]) {
    absl::StatusOr<Move> move = MoveFromJson(move_json, game);
    if (!move.ok()) return move.status();
    CHECK(game.MakeMove(*move));
    moves.push_back(*move);
  }
  absl::Time updated;
  std::string error;
  if (!absl::ParseTime(absl::RFC3339_full, saved["updated"].get<std::string>(),
                       &updated, &error)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Bad \"updated\" time: ", error));
  }
  absl::StatusOr<std::unique_ptr<GameSession>> session =
      Create(saved["id"].get<std::string>(), *std::move(config), moves,
             std::move(ai_factory), std::move(save_path),
             std::move(on_change));
  if (session.ok()) {
    (*session)->updated_ = updated;
    (*session)->Save();
  }
  return session;
}

json GameSession::State() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return StateLocked();
}

json GameSession::StateLocked() const {
  json state = GameToJson(game_);
  state["id"] = id_;
  state["version"] = version_;
  state["seats"] = GameConfigToJson(config_)["seats"];
  state["ai_thinking"] = NeedsAiTurnLocked();
  state["can_undo"] = UndoTarget() >= 0;
  return state;
}

json GameSession::Summary() const {
  std::lock_guard<std::mutex> lock(mutex_);
  const json state = GameToJson(game_);
  json summary = {{"id", id_},
                  {"num_players", config_.num_players},
                  {"seats", GameConfigToJson(config_)["seats"]},
                  {"finished", state["finished"]},
                  {"num_moves", game_.moves().size()},
                  {"scores", state["scores"]},
                  {"updated", TimeToString(updated_)}};
  if (state.contains("winners")) summary["winners"] = state["winners"];
  return summary;
}

json GameSession::WaitForState(int version, absl::Duration timeout) {
  std::unique_lock<std::mutex> lock(mutex_);
  changed_.wait_for(lock, absl::ToChronoNanoseconds(timeout),
                    [&] { return version_ > version || closed_; });
  return StateLocked();
}

json GameSession::LegalMoves() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return {{"version", version_}, {"moves", PossibleMovesToJson(game_)}};
}

absl::Status GameSession::PlayMove(int version, const json& move_json) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) return absl::FailedPreconditionError("The game is closed");
    if (version != version_) {
      return absl::FailedPreconditionError("The game has changed");
    }
    if (game_.Finished()) {
      return absl::FailedPreconditionError("The game is finished");
    }
    if (!config_.seats[game_.current_player()].human) {
      return absl::FailedPreconditionError("It isn't a human's turn");
    }
    absl::StatusOr<Move> move = MoveFromJson(move_json, game_);
    if (!move.ok()) return move.status();
    CHECK(game_.MakeMove(*move)) << move->DebugString();
    PlayForcedPasses();
    Changed();
  }
  if (on_change_) on_change_();
  return absl::OkStatus();
}

absl::Status GameSession::Undo(int version) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) return absl::FailedPreconditionError("The game is closed");
    if (version != version_) {
      return absl::FailedPreconditionError("The game has changed");
    }
    const int target = UndoTarget();
    if (target < 0) {
      return absl::FailedPreconditionError("No human move to take back");
    }
    Game game(config_.num_players);
    for (int i = 0; i < target; ++i) {
      CHECK(game.MakeMove(game_.moves()[i]));
    }
    game_ = std::move(game);
    ResetAis();
    Changed();
  }
  if (on_change_) on_change_();
  return absl::OkStatus();
}

bool GameSession::NeedsAiTurn() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return NeedsAiTurnLocked();
}

bool GameSession::NeedsAiTurnLocked() const {
  return !closed_ && !game_.Finished() &&
         !config_.seats[game_.current_player()].human;
}

void GameSession::RunAiTurn() {
  std::shared_ptr<Player> ai;
  Game game(config_.num_players);
  int version;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!NeedsAiTurnLocked()) return;
    ai = ais_[game_.current_player()];
    game = game_;
    version = version_;
  }
  const Move move = ai->SelectMove(game);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    // The search is stale if a human undid moves while it ran.
    if (closed_ || version != version_) return;
    CHECK(game_.MakeMove(move)) << move.DebugString();
    PlayForcedPasses();
    Changed();
  }
  if (on_change_) on_change_();
}

void GameSession::Close() {
  std::lock_guard<std::mutex> lock(mutex_);
  closed_ = true;
  changed_.notify_all();
}

json GameSession::SaveJson() const {
  json moves = json::array();
  for (const Move& move : game_.moves()) moves.push_back(MoveToJson(move));
  return {{"id", id_},
          {"config", GameConfigToJson(config_)},
          {"moves", std::move(moves)},
          {"updated", TimeToString(updated_)}};
}

void GameSession::ResetAis() {
  ais_.assign(config_.num_players, nullptr);
  for (int player = 0; player < config_.num_players; ++player) {
    if (!config_.seats[player].human) {
      ais_[player] = ai_factory_(player, config_.seats[player]);
    }
  }
}

void GameSession::PlayForcedPasses() {
  while (!game_.Finished() && game_.NumPossibleMoves() == 0) {
    CHECK(game_.MakeMove(Move::EmptyMove(game_.current_color())));
  }
}

void GameSession::Changed() {
  ++version_;
  updated_ = absl::Now();
  Save();
  changed_.notify_all();
}

int GameSession::UndoTarget() const {
  const std::vector<Move>& moves = game_.moves();
  for (int i = static_cast<int>(moves.size()) - 1; i >= 0; --i) {
    const int player = PlayerForColor(config_.num_players, moves[i].color);
    if (moves[i].tile != -1 && config_.seats[player].human) return i;
  }
  return -1;
}

void GameSession::Save() const {
  if (save_path_.empty() || closed_) return;
  // Write a temporary file and rename it, so a crash never leaves a
  // half-written save.
  const std::string temp_path = save_path_ + ".tmp";
  {
    std::ofstream file(temp_path);
    file << SaveJson().dump(2) << "\n";
    if (!file) {
      LOG(ERROR) << "Couldn't write " << temp_path;
      return;
    }
  }
  if (std::rename(temp_path.c_str(), save_path_.c_str()) != 0) {
    LOG(ERROR) << "Couldn't rename " << temp_path << " to " << save_path_;
  }
}

}  // namespace blokus
