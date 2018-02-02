#include <list>
#include <memory>
#include <mutex>
#include <set>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <jsoncpp/json/writer.h>

#include "game/game.h"
#include "util/http_server.h"

namespace blokus {

class RandomAI : public Player {
 public:
  RandomAI(Color color) : Player(color) {}

  bool SelectMove(const Board& board, Move* move, int* chosen_tile) {
    for (int tile = 20; tile >= 0; tile --) {
      if (played_tiles.count(tile) > 0) continue;
      auto moves = board.PossibleMoves(kTiles[tile], color());
      if (moves.size() > 0) {
        *move = moves[rand() % moves.size()];
        *chosen_tile = tile;
        played_tiles.insert(tile);
        return true;
      }
    }
    return false;
  }

 private:
  std::set<int> played_tiles;
};

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
  
  bool SelectMove(const Board& board __attribute__ ((unused)),
                  Move* move, int* chosen_tile) {
    // Block until we have a move ready.
    LOG(INFO) << ColorToString(color()) << " is waiting for a UI move.";
    while (true) {
      std::lock_guard<std::mutex> lock(m_);
      if (move_ready_) break;
    }
    *move = move_;
    *chosen_tile = chosen_tile_;
    // No locking, since the server won't make more requests until after we've
    // played this move.
    move_ready_ = false;
    return true;
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
    chosen_tile_ = data["tile"].asInt();
    move_.rotation = data["move"]["rotation"].asInt();
    move_.flip = data["move"]["flip"].asInt();
    move_.coord[0] = data["move"]["coord"][0].asInt();
    move_.coord[1] = data["move"]["coord"][1].asInt();
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
  int chosen_tile_;
  bool move_ready_ = false;
  std::mutex m_;
};

// TODO(piotrf): how does this interact with multiple clients vs
// one client with several players?
class MoveForwarder {
 public:
  void Handle(blokus::HttpServer::Request* request) {
    std::pair<Move, int> next_move;
    while (true) {
      std::lock_guard<std::mutex> lock(m_);
      if (!moves_.empty()) {
        next_move = moves_.front();
        moves_.pop_front();
        break;
      }
    }
    Json::Value data;
    data["tile"] = next_move.second;
    data["move"]["rotation"] = Json::Int(next_move.first.rotation);
    data["move"]["flip"] = next_move.first.flip;
    data["move"]["coord"][0] = Json::Int(next_move.first.coord[0]);
    data["move"]["coord"][1] = Json::Int(next_move.first.coord[1]);
    Json::FastWriter writer;
    request->set_response(writer.write(data));
    request->set_status(200);
  }

  void NewMove(const Move& move, int tile) {
    std::lock_guard<std::mutex> lock(m_);
    moves_.emplace_back(move, tile);
  }
  
 private:
  std::list<std::pair<Move, int>> moves_;
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

  blokus::Game game;
  game.AddPlayer(blokus::BLUE,
                 std::unique_ptr<blokus::WebPlayer>(
                     new blokus::WebPlayer(blokus::BLUE, &server)));
  game.AddPlayer(blokus::YELLOW,
                 std::unique_ptr<blokus::RandomAI>(
                     new blokus::RandomAI(blokus::YELLOW)));
  game.AddPlayer(blokus::RED,
                 std::unique_ptr<blokus::RandomAI>(
                     new blokus::RandomAI(blokus::RED)));
  game.AddPlayer(blokus::GREEN,
                 std::unique_ptr<blokus::RandomAI>(
                     new blokus::RandomAI(blokus::GREEN)));

  blokus::MoveForwarder forwarder;
  game.AddObserver([&forwarder](const blokus::Move& move, int tile) {
      forwarder.NewMove(move, tile);
    });
  server.RegisterHandler(
      "/game/next_move",
      [&forwarder](blokus::HttpServer::Request* request) {
        forwarder.Handle(request);
      });
  
  game.Play();
  
  return 0;
}
