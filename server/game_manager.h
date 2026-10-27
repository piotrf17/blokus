#ifndef BLOKUS_SERVER_GAME_MANAGER_H_
#define BLOKUS_SERVER_GAME_MANAGER_H_

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"

#include "server/game_session.h"

namespace blokus {

// All the games being played, and the thread that plays their AI turns.
// Thread-safe.
//
// AI turns run one at a time, taking the games in turn, since one MCTS search
// already uses every core.
class GameManager {
 public:
  struct Options {
    // Where games are saved, as <id>.json. Created if it doesn't exist. If
    // empty, games are only kept in memory.
    std::string data_dir;
    AiFactory ai_factory = MctsAiFactory();
  };

  // Loads the games saved in `options.data_dir`, skipping (with a warning)
  // any that can't be loaded, and starts the AI thread.
  explicit GameManager(Options options);

  // Closes every game, and waits for any AI search in progress to finish.
  ~GameManager();

  GameManager(const GameManager&) = delete;
  GameManager& operator=(const GameManager&) = delete;

  // Starts a game from GameConfigFromJson's format. Returns InvalidArgument
  // for a bad config.
  absl::StatusOr<std::shared_ptr<GameSession>> CreateGame(
      const nlohmann::json& config);

  // Returns the game with `id`, or null if there isn't one.
  std::shared_ptr<GameSession> GetGame(const std::string& id) const;

  // Every game's Summary(), most recently updated first.
  nlohmann::json ListGames() const;

  // Closes the game and deletes its save. Returns NotFound if there's no game
  // with `id`.
  absl::Status DeleteGame(const std::string& id);

  // Closes every game, for shutting down: waiting clients get the current
  // state, and no more moves are played.
  void CloseGames();

 private:
  void LoadGames();
  std::string SavePath(const std::string& id) const;
  GameSession::ChangeCallback OnChange();
  // Tells the AI thread to look for games that need an AI turn.
  void WakeAiThread();
  void RunAiTurns();

  const Options options_;

  mutable std::mutex mutex_;
  std::condition_variable ai_work_;
  std::map<std::string, std::shared_ptr<GameSession>> games_;
  std::mt19937_64 id_rng_;
  // Set when a game may need an AI turn.
  bool ai_work_pending_ = true;
  // The game that had the last AI turn, to take games in turn.
  std::string last_ai_game_;
  bool stopping_ = false;
  std::thread ai_thread_;
};

}  // namespace blokus

#endif  // BLOKUS_SERVER_GAME_MANAGER_H_
