#ifndef BLOKUS_SERVER_GAME_SESSION_H_
#define BLOKUS_SERVER_GAME_SESSION_H_

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "nlohmann/json.hpp"

#include "game/game.h"
#include "game/player.h"
#include "server/config.h"

namespace blokus {

// Makes the player for an AI seat. Tests substitute fake players.
using AiFactory =
    std::function<std::unique_ptr<Player>(int player_id, const SeatConfig&)>;

// Returns MctsAIs with the seat's options.
AiFactory MctsAiFactory();

// One game being played through the web API. Thread-safe.
//
// Humans move through PlayMove. AI turns are played by calling RunAiTurn
// whenever NeedsAiTurn is true; GameManager does that from a single thread.
// A color with no legal moves passes automatically, whoever plays it.
//
// Every change bumps the version. Clients send the version they last saw
// with each change, so a change based on a stale state is rejected, and
// long-poll for the next version with WaitForState.
class GameSession {
 public:
  // Called after every change, without the session's lock held.
  using ChangeCallback = std::function<void()>;

  // Starts a game, or resumes one that has played `moves` (from SaveJson).
  // The moves must be legal. If `save_path` isn't empty, the session saves
  // itself there after every change.
  static absl::StatusOr<std::unique_ptr<GameSession>> Create(
      std::string id, GameConfig config, const std::vector<Move>& moves,
      AiFactory ai_factory, std::string save_path = "",
      ChangeCallback on_change = nullptr);

  // Restores a session from SaveJson's output.
  static absl::StatusOr<std::unique_ptr<GameSession>> Load(
      const nlohmann::json& saved, AiFactory ai_factory,
      std::string save_path = "", ChangeCallback on_change = nullptr);

  GameSession(const GameSession&) = delete;
  GameSession& operator=(const GameSession&) = delete;

  const std::string& id() const { return id_; }

  // The game state for clients: GameToJson, plus "id", "version", "seats"
  // (GameConfigToJson's), "ai_thinking" (whether it's an AI's turn) and
  // "can_undo".
  nlohmann::json State() const;

  // A summary for listing games: "id", "num_players", "seats", "finished",
  // "num_moves", "scores", "winners" (if finished) and "updated".
  nlohmann::json Summary() const;

  // Waits until the version is greater than `version`, `timeout` passes, or
  // the session closes, then returns State().
  nlohmann::json WaitForState(int version, absl::Duration timeout);

  // {"version": ..., "moves": PossibleMovesToJson(...)}.
  nlohmann::json LegalMoves() const;

  // Plays a human's move, parsed by MoveFromJson. Returns FailedPrecondition
  // if `version` is stale or it isn't a human's turn, and InvalidArgument for
  // an illegal move.
  absl::Status PlayMove(int version, const nlohmann::json& move);

  // Takes back moves up to and including the last tile a human seat played,
  // so that it's that human's turn again. (Humans never choose to pass: a
  // pass is only legal without moves, and then it's automatic.)
  // Any AI search in progress is discarded. Returns FailedPrecondition if
  // `version` is stale or no human has moved.
  absl::Status Undo(int version);

  // True if it's an AI's turn.
  bool NeedsAiTurn() const;

  // Asks the AI whose turn it is for a move, and plays it, unless the game
  // changed (e.g. through Undo) or closed during the search. Does nothing if
  // it isn't an AI's turn. Blocks for the length of the search. Must not be
  // called again for this session until it returns, since an AI can only
  // run one search at a time.
  void RunAiTurn();

  // Stops saving and playing AI turns, and wakes up WaitForState callers.
  void Close();

  // The game as saved to disk: {"id", "config", "moves", "updated"}.
  nlohmann::json SaveJson() const;

 private:
  GameSession(std::string id, GameConfig config, AiFactory ai_factory,
              std::string save_path, ChangeCallback on_change);

  // Remakes the AI players, so they don't reuse trees from before an undo.
  void ResetAis();
  // Plays passes for colors that have no moves.
  void PlayForcedPasses();
  // Bumps the version, saves, and wakes waiters. Call with mutex_ held, then
  // call on_change_ after releasing it.
  void Changed();
  bool NeedsAiTurnLocked() const;
  // The number of moves to keep for Undo, or -1 if there's nothing to undo.
  int UndoTarget() const;
  nlohmann::json StateLocked() const;
  void Save() const;

  const std::string id_;
  const GameConfig config_;
  const AiFactory ai_factory_;
  const std::string save_path_;
  const ChangeCallback on_change_;

  mutable std::mutex mutex_;
  std::condition_variable changed_;
  Game game_;
  // Indexed by player id. Null for human seats. Shared so that RunAiTurn can
  // search without the lock while Undo replaces them.
  std::vector<std::shared_ptr<Player>> ais_;
  int version_ = 0;
  absl::Time updated_;
  bool closed_ = false;
};

}  // namespace blokus

#endif  // BLOKUS_SERVER_GAME_SESSION_H_
