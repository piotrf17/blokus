#include <chrono>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

#include "absl/memory/memory.h"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include <jsoncpp/json/writer.h>

#include "ai/random.h"
#include "game/game_runner.h"
#include "util/http_server.h"

namespace blokus {

class WebPlayer : public Player {
 public:
  WebPlayer(Color color, HttpServer* server) :
      Player(color), server_(server) {
    server_->RegisterHandler(
        "/game/" + ColorToString(color) + "/place",
        [this](HttpServer::Request* request) {
          this->Handle(request);
        });
  }
  
  Move SelectMove(const Game& game __attribute__ ((unused))) {
    // Block until we have a move ready.
    LOG(INFO) << ColorToString(color()) << " is waiting for a UI move.";
    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      std::lock_guard<std::mutex> lock(m_);
      if (move_ready_) break;
    }
    // No locking, since the server won't make more requests until after we've
    // played this move.
    move_ready_ = false;
    // TODO(piotrf): return "pass" from frontend.
    return move_;
  }

  void Handle(blokus::HttpServer::Request* request) {
    // Verify that there is no move waiting. There should be no way to get here
    // unless we've already played our move, unless there is a bug in the UI.
    {
      std::lock_guard<std::mutex> lock(m_);
      CHECK(!move_ready_);
    }
    // Parse the move out of the json.
    const Json::Value& data = request->json();
    move_.color = color();
    move_.tile = data["tile"].asInt();
    move_.placement.rotation = data["move"]["rotation"].asInt();
    move_.placement.flip = data["move"]["flip"].asInt();
    move_.placement.coord[0] = data["move"]["coord"][0].asInt();
    move_.placement.coord[1] = data["move"]["coord"][1].asInt();
    // Indicate that we have a move ready.
    {
      std::unique_lock<std::mutex> lock(m_, std::defer_lock);
      move_ready_ = true;
    }
    // TODO(piotrf): set status based on move result?
    request->set_status(204);
  }

 private:
  blokus::HttpServer* server_;
  Move move_;
  bool move_ready_ = false;
  std::mutex m_;
};

// TODO(piotrf): how does this interact with multiple clients vs
// one client with several players?
class MoveForwarder {
 public:
  void Handle(blokus::HttpServer::Request* request) {
    Move next_move;
    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      std::lock_guard<std::mutex> lock(m_);
      if (!moves_.empty()) {
        next_move = moves_.front();
        moves_.pop_front();
        break;
      }
    }
    Json::Value data;
    data["tile"] = next_move.tile;
    data["move"]["rotation"] = Json::Int(next_move.placement.rotation);
    data["move"]["flip"] = next_move.placement.flip;
    data["move"]["coord"][0] = Json::Int(next_move.placement.coord[0]);
    data["move"]["coord"][1] = Json::Int(next_move.placement.coord[1]);
    Json::FastWriter writer;
    request->set_response(writer.write(data));
    request->set_status(200);
  }

  void NewMove(const Move& move) {
    std::lock_guard<std::mutex> lock(m_);
    moves_.emplace_back(move);
  }
  
 private:
  std::list<Move> moves_;
  std::mutex m_;
};

}  // namespace blokus

int main(int argc, char **argv) {
  // Initialize command line flags and logging.
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);  

  blokus::HttpServer server;
  server.RegisterStaticHandler("/", "main/static/index.html");
  server.RegisterStaticHandler("/blokus.js", "main/static/blokus.js");
  server.RegisterStaticHandler("/tiles.js", "main/static/tiles.js");
  server.RegisterStaticHandler("/board.js", "main/static/board.js");
  server.RegisterStaticHandler("/jquery-3.1.0.js",
                               "main/static/jquery-3.1.0.js");
  
  CHECK(server.Start());
  LOG(INFO) << "Server is running on localhost:7777";

  blokus::GameRunner game;
  game.AddPlayer(blokus::BLUE,
                 absl::make_unique<blokus::WebPlayer>(blokus::BLUE, &server));
  game.AddPlayer(blokus::YELLOW,
                 absl::make_unique<blokus::RandomAI>(blokus::YELLOW));
  game.AddPlayer(blokus::RED,
                 absl::make_unique<blokus::RandomAI>(blokus::RED));
  game.AddPlayer(blokus::GREEN,
                 absl::make_unique<blokus::RandomAI>(blokus::GREEN));  

  blokus::MoveForwarder forwarder;
  game.AddObserver([&forwarder](const blokus::Board& board,
                                const blokus::Move& move) {
      forwarder.NewMove(move);
    });
  game.AddObserver(blokus::BoardPrintingObserver());
  server.RegisterHandler(
      "/game/next_move",
      [&forwarder](blokus::HttpServer::Request* request) {
        forwarder.Handle(request);
      });
  
  game.Play();
  
  return 0;
}
