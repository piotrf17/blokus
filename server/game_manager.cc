#include "server/game_manager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

#include "server/config.h"

namespace blokus {
namespace {

using ::nlohmann::json;

}  // namespace

GameManager::GameManager(Options options)
    : options_(std::move(options)), id_rng_(std::random_device()()) {
  if (!options_.data_dir.empty()) LoadGames();
  ai_thread_ = std::thread([this] { RunAiTurns(); });
}

GameManager::~GameManager() {
  CloseGames();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
  }
  ai_work_.notify_all();
  ai_thread_.join();
}

void GameManager::CloseGames() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& [id, game] : games_) game->Close();
}

absl::StatusOr<std::shared_ptr<GameSession>> GameManager::CreateGame(
    const json& config_json) {
  absl::StatusOr<GameConfig> config = GameConfigFromJson(config_json);
  if (!config.ok()) return config.status();

  std::shared_ptr<GameSession> game;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id;
    do {
      id = absl::StrFormat("%012x", id_rng_() & 0xffffffffffff);
    } while (games_.contains(id));
    absl::StatusOr<std::unique_ptr<GameSession>> session =
        GameSession::Create(id, *std::move(config), {}, options_.ai_factory,
                            SavePath(id), OnChange());
    if (!session.ok()) return session.status();
    game = *std::move(session);
    games_[id] = game;
  }
  WakeAiThread();
  return game;
}

std::shared_ptr<GameSession> GameManager::GetGame(const std::string& id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = games_.find(id);
  return it == games_.end() ? nullptr : it->second;
}

json GameManager::ListGames() const {
  std::vector<json> summaries;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [id, game] : games_) {
      summaries.push_back(game->Summary());
    }
  }
  // RFC 3339 times in UTC sort as strings.
  std::sort(summaries.begin(), summaries.end(),
            [](const json& a, const json& b) {
              return a["updated"].get<std::string>() >
                     b["updated"].get<std::string>();
            });
  return summaries;
}

absl::Status GameManager::DeleteGame(const std::string& id) {
  std::shared_ptr<GameSession> game;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = games_.find(id);
    if (it == games_.end()) {
      return absl::NotFoundError(absl::StrCat("No game ", id));
    }
    game = std::move(it->second);
    games_.erase(it);
  }
  game->Close();
  if (!options_.data_dir.empty()) {
    std::error_code error;
    std::filesystem::remove(SavePath(id), error);
    if (error) {
      LOG(ERROR) << "Couldn't delete " << SavePath(id) << ": "
                 << error.message();
    }
  }
  return absl::OkStatus();
}

void GameManager::LoadGames() {
  std::filesystem::create_directories(options_.data_dir);
  for (const auto& entry :
       std::filesystem::directory_iterator(options_.data_dir)) {
    const std::filesystem::path& path = entry.path();
    if (path.extension() != ".json") continue;
    std::ifstream file(path);
    const json saved = json::parse(file, nullptr, /*allow_exceptions=*/false);
    // The id names the save file, so it must match.
    if (saved.is_discarded() || !saved.is_object() || !saved.contains("id") ||
        saved["id"] != path.stem().string()) {
      LOG(WARNING) << "Skipping " << path << ": not a saved game";
      continue;
    }
    const std::string id = path.stem().string();
    absl::StatusOr<std::unique_ptr<GameSession>> session = GameSession::Load(
        saved, options_.ai_factory, SavePath(id), OnChange());
    if (!session.ok()) {
      LOG(WARNING) << "Skipping " << path << ": " << session.status();
      continue;
    }
    games_[id] = *std::move(session);
  }
  LOG(INFO) << "Loaded " << games_.size() << " games from "
            << options_.data_dir;
}

std::string GameManager::SavePath(const std::string& id) const {
  if (options_.data_dir.empty()) return "";
  return (std::filesystem::path(options_.data_dir) / (id + ".json")).string();
}

GameSession::ChangeCallback GameManager::OnChange() {
  return [this] { WakeAiThread(); };
}

void GameManager::WakeAiThread() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ai_work_pending_ = true;
  }
  ai_work_.notify_all();
}

void GameManager::RunAiTurns() {
  while (true) {
    std::shared_ptr<GameSession> next;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      ai_work_.wait(lock, [this] { return stopping_ || ai_work_pending_; });
      if (stopping_) return;
      ai_work_pending_ = false;
      // The first game after the last one served that needs an AI turn,
      // wrapping around.
      auto it = games_.upper_bound(last_ai_game_);
      for (int i = 0; i < static_cast<int>(games_.size()); ++i, ++it) {
        if (it == games_.end()) it = games_.begin();
        if (it->second->NeedsAiTurn()) {
          next = it->second;
          break;
        }
      }
      if (next == nullptr) continue;
      last_ai_game_ = next->id();
      // Other games may need turns too, so look again afterwards.
      ai_work_pending_ = true;
    }
    next->RunAiTurn();
  }
}

}  // namespace blokus
