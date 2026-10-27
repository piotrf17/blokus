#include "server/web_server.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "httplib.h"
#include "nlohmann/json.hpp"

#include "server/game_manager.h"
#include "server/test_ai.h"

namespace blokus {
namespace {

using ::nlohmann::json;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::Lt;
using ::testing::SizeIs;

const char kHumanVsAi[] = R"(
    {"num_players": 2,
     "seats": [{"type": "human"}, {"type": "ai", "preset": "low"}]})";
const char kHumans[] = R"(
    {"num_players": 2, "seats": [{"type": "human"}, {"type": "human"}]})";

struct Response {
  int status = 0;
  json body;
};

class WebServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const std::filesystem::path dir =
        std::filesystem::path(::testing::TempDir()) / "static";
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "index.html") << "the index";
    std::ofstream(dir / "game.html") << "the game";
    std::ofstream(dir / "app.js") << "the script";

    manager_ = std::make_unique<GameManager>(
        GameManager::Options{.ai_factory = FirstMoveAiFactory()});
    server_ = std::make_unique<WebServer>(
        manager_.get(),
        WebServer::Options{.static_dir = dir.string(),
                           .long_poll_timeout = absl::Milliseconds(300)});
    const int port = server_->BindToAnyPort("127.0.0.1");
    ASSERT_THAT(port, Gt(0));
    thread_ = std::thread([this] { server_->ListenAfterBind(); });
    client_ = std::make_unique<httplib::Client>("127.0.0.1", port);
  }

  void TearDown() override {
    manager_->CloseGames();
    server_->Stop();
    thread_.join();
  }

  static Response ToResponse(const httplib::Result& result) {
    if (!result) return {};
    return {result->status,
            json::parse(result->body, nullptr, /*allow_exceptions=*/false)};
  }

  Response Get(const std::string& path) {
    return ToResponse(client_->Get(path));
  }
  Response Post(const std::string& path, const std::string& body) {
    return ToResponse(client_->Post(path, body, "application/json"));
  }
  Response Post(const std::string& path, const json& body) {
    return Post(path, body.dump());
  }
  Response Delete(const std::string& path) {
    return ToResponse(client_->Delete(path));
  }

  // Creates a game and returns its id.
  std::string CreateGame(const char* config) {
    const Response response = Post("/api/games", std::string(config));
    EXPECT_THAT(response.status, Eq(201)) << response.body;
    return response.body["id"].get<std::string>();
  }

  // Plays the first legal move for a human, returning the response.
  Response PlayFirstMove(const std::string& id) {
    const Response legal = Get(absl::StrCat("/api/games/", id, "/legal_moves"));
    json move = legal.body["moves"][0];
    move["version"] = legal.body["version"];
    return Post(absl::StrCat("/api/games/", id, "/moves"), move);
  }

  std::unique_ptr<GameManager> manager_;
  std::unique_ptr<WebServer> server_;
  std::thread thread_;
  std::unique_ptr<httplib::Client> client_;
};

TEST_F(WebServerTest, Presets) {
  const Response response = Get("/api/presets");
  EXPECT_THAT(response.status, Eq(200));
  EXPECT_THAT(response.body["medium"]["num_iterations"], Eq(3000));
}

TEST_F(WebServerTest, Tiles) {
  const Response response = Get("/api/tiles");
  EXPECT_THAT(response.status, Eq(200));
  EXPECT_THAT(response.body, SizeIs(21));
}

TEST_F(WebServerTest, CreateAndGetGame) {
  const Response created = Post("/api/games", std::string(kHumanVsAi));
  ASSERT_THAT(created.status, Eq(201));
  EXPECT_THAT(created.body["version"], Eq(0));
  EXPECT_THAT(created.body["seats"][1]["preset"], Eq("low"));
  const std::string id = created.body["id"].get<std::string>();

  const Response game = Get("/api/games/" + id);
  EXPECT_THAT(game.status, Eq(200));
  EXPECT_THAT(game.body, Eq(created.body));

  const Response list = Get("/api/games");
  ASSERT_THAT(list.body, SizeIs(1));
  EXPECT_THAT(list.body[0]["id"], Eq(id));
}

TEST_F(WebServerTest, CreateRejectsBadRequests) {
  Response response = Post("/api/games", std::string("{not json"));
  EXPECT_THAT(response.status, Eq(400));
  EXPECT_TRUE(response.body.contains("error"));
  response = Post("/api/games", std::string(R"({"num_players": 3})"));
  EXPECT_THAT(response.status, Eq(400));
  EXPECT_THAT(response.body["error"], Eq("\"num_players\" must be 2 or 4"));
}

TEST_F(WebServerTest, UnknownGame) {
  EXPECT_THAT(Get("/api/games/abc123").status, Eq(404));
  EXPECT_THAT(Get("/api/games/abc123/legal_moves").status, Eq(404));
  EXPECT_THAT(Post("/api/games/abc123/undo", json{{"version", 0}}).status,
              Eq(404));
  EXPECT_THAT(Delete("/api/games/abc123").status, Eq(404));
  // Not even a valid id.
  EXPECT_THAT(Get("/api/games/NOT-AN-ID").status, Eq(404));
}

TEST_F(WebServerTest, PlayAndAiReply) {
  const std::string id = CreateGame(kHumanVsAi);
  const Response played = PlayFirstMove(id);
  ASSERT_THAT(played.status, Eq(200)) << played.body;
  EXPECT_THAT(played.body["moves"], SizeIs(1));
  // The long-poll returns once the AI has replied.
  const int version = played.body["version"].get<int>();
  Response state =
      Get(absl::StrCat("/api/games/", id, "?wait_version=", version));
  while (state.body["ai_thinking"] == true) {
    state = Get(absl::StrCat("/api/games/", id, "?wait_version=",
                             state.body["version"].get<int>()));
  }
  EXPECT_THAT(state.body["moves"], SizeIs(2));
  EXPECT_THAT(state.body["current_color"], Eq("red"));
}

TEST_F(WebServerTest, MoveErrors) {
  const std::string id = CreateGame(kHumans);
  const std::string path = absl::StrCat("/api/games/", id, "/moves");
  json move = Get(absl::StrCat("/api/games/", id, "/legal_moves"))
                  .body["moves"][0];
  // No version.
  EXPECT_THAT(Post(path, move).status, Eq(400));
  // Stale version.
  move["version"] = 5;
  EXPECT_THAT(Post(path, move).status, Eq(409));
  // Illegal.
  EXPECT_THAT(Post(path, json::parse(R"({"version": 0, "tile": 0,
                                         "cells": [[9, 9]]})"))
                  .status,
              Eq(400));
  // Early pass.
  EXPECT_THAT(Post(path, json::parse(R"({"version": 0, "pass": true})"))
                  .status,
              Eq(400));
  EXPECT_THAT(Get("/api/games/" + id).body["version"], Eq(0));
}

TEST_F(WebServerTest, Undo) {
  const std::string id = CreateGame(kHumans);
  ASSERT_THAT(PlayFirstMove(id).status, Eq(200));
  const std::string path = absl::StrCat("/api/games/", id, "/undo");
  EXPECT_THAT(Post(path, json{{"version", 0}}).status, Eq(409));
  const Response undone = Post(path, json{{"version", 1}});
  ASSERT_THAT(undone.status, Eq(200)) << undone.body;
  EXPECT_THAT(undone.body["moves"], SizeIs(0));
  EXPECT_THAT(undone.body["version"], Eq(2));
}

TEST_F(WebServerTest, Position) {
  const std::string id = CreateGame(kHumans);
  ASSERT_THAT(PlayFirstMove(id).status, Eq(200));
  const std::string path = absl::StrCat("/api/games/", id, "/position");
  Response position = Get(path + "?moves=0");
  ASSERT_THAT(position.status, Eq(200)) << position.body;
  EXPECT_THAT(position.body["num_moves"], Eq(0));
  EXPECT_THAT(position.body["moves"], SizeIs(0));
  position = Get(path + "?moves=1");
  EXPECT_THAT(position.body["moves"], SizeIs(1));
  EXPECT_THAT(Get(path + "?moves=2").status, Eq(400));
  EXPECT_THAT(Get(path + "?moves=x").status, Eq(400));
  EXPECT_THAT(Get(path).status, Eq(400));
}

TEST_F(WebServerTest, LongPollTimesOut) {
  const std::string id = CreateGame(kHumans);
  const absl::Time start = absl::Now();
  const Response state = Get(absl::StrCat("/api/games/", id, "?wait_version=0"));
  EXPECT_THAT(state.status, Eq(200));
  EXPECT_THAT(state.body["version"], Eq(0));
  EXPECT_THAT(absl::Now() - start, testing::Ge(absl::Milliseconds(300)));
  EXPECT_THAT(Get(absl::StrCat("/api/games/", id, "?wait_version=x")).status,
              Eq(400));
}

TEST_F(WebServerTest, ClosingGamesEndsLongPolls) {
  // A long-poll that would otherwise wait for 30 s.
  WebServer slow_server(manager_.get(),
                        {.long_poll_timeout = absl::Seconds(30)});
  const int port = slow_server.BindToAnyPort("127.0.0.1");
  ASSERT_THAT(port, Gt(0));
  std::thread thread([&] { slow_server.ListenAfterBind(); });
  const std::string id = CreateGame(kHumans);
  httplib::Client client("127.0.0.1", port);
  const absl::Time start = absl::Now();
  std::thread closer([&] {
    absl::SleepFor(absl::Milliseconds(100));
    manager_->CloseGames();
  });
  const httplib::Result result =
      client.Get(absl::StrCat("/api/games/", id, "?wait_version=0"));
  closer.join();
  EXPECT_TRUE(result);
  EXPECT_THAT(absl::Now() - start, Lt(absl::Seconds(10)));
  slow_server.Stop();
  thread.join();
}

TEST_F(WebServerTest, DeleteGame) {
  const std::string id = CreateGame(kHumans);
  EXPECT_THAT(Delete("/api/games/" + id).status, Eq(200));
  EXPECT_THAT(Get("/api/games/" + id).status, Eq(404));
  EXPECT_THAT(Get("/api/games").body, SizeIs(0));
}

TEST_F(WebServerTest, StaticFiles) {
  httplib::Result result = client_->Get("/");
  ASSERT_TRUE(result);
  EXPECT_THAT(result->status, Eq(200));
  EXPECT_THAT(result->body, Eq("the index"));
  result = client_->Get("/game/abc123");
  ASSERT_TRUE(result);
  EXPECT_THAT(result->body, Eq("the game"));
  result = client_->Get("/static/app.js");
  ASSERT_TRUE(result);
  EXPECT_THAT(result->body, Eq("the script"));
  result = client_->Get("/static/missing.js");
  ASSERT_TRUE(result);
  EXPECT_THAT(result->status, Eq(404));
}

}  // namespace
}  // namespace blokus
