#ifndef BLOKUS_SERVER_TEST_AI_H_
#define BLOKUS_SERVER_TEST_AI_H_

#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

#include "game/game.h"
#include "game/player.h"
#include "server/game_session.h"

namespace blokus {

// Lets a test hold fake AIs in the middle of a search.
class Gate {
 public:
  // Blocks until Open().
  void Wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    ++num_waiting_;
    changed_.notify_all();
    changed_.wait(lock, [this] { return open_; });
  }

  // Blocks until an AI is waiting at the gate.
  void WaitForAi() {
    std::unique_lock<std::mutex> lock(mutex_);
    changed_.wait(lock, [this] { return num_waiting_ > 0; });
  }

  void Open() {
    std::lock_guard<std::mutex> lock(mutex_);
    open_ = true;
    changed_.notify_all();
  }

 private:
  std::mutex mutex_;
  std::condition_variable changed_;
  int num_waiting_ = 0;
  bool open_ = false;
};

// A fast AI that plays the first legal move, after waiting at `gate` if
// there is one.
class FirstMoveAi : public Player {
 public:
  FirstMoveAi(int player_id, Gate* gate) : Player(player_id), gate_(gate) {}

  Move SelectMove(const Game& game) override {
    if (gate_ != nullptr) gate_->Wait();
    const std::vector<Move> moves = game.PossibleMoves();
    return moves.empty() ? Move::EmptyMove(game.current_color())
                         : moves.front();
  }

 private:
  Gate* gate_;
};

inline AiFactory FirstMoveAiFactory(Gate* gate = nullptr) {
  return [gate](int player_id, const SeatConfig&) {
    return std::make_unique<FirstMoveAi>(player_id, gate);
  };
}

}  // namespace blokus

#endif  // BLOKUS_SERVER_TEST_AI_H_
