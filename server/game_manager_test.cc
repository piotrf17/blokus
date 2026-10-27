#include "server/game_manager.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "server/test_ai.h"

namespace blokus {
namespace {

using ::nlohmann::json;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::SizeIs;

const char kHumanVsAi[] = R"(
    {"num_players": 2,
     "seats": [{"type": "human"}, {"type": "ai", "preset": "low"}]})";
const char kAiVsAi[] = R"(
    {"num_players": 2,
     "seats": [{"type": "ai", "preset": "low"},
               {"type": "ai", "preset": "low"}]})";

GameManager::Options TestOptions(std::string data_dir = "") {
  return {.data_dir = std::move(data_dir),
          .ai_factory = FirstMoveAiFactory()};
}

// A fresh, empty directory for one test's saves.
std::string MakeDataDir(const std::string& name) {
  const std::filesystem::path dir =
      std::filesystem::path(::testing::TempDir()) / name;
  std::filesystem::remove_all(dir);
  return dir.string();
}

std::shared_ptr<GameSession> Create(GameManager& manager, const char* config) {
  absl::StatusOr<std::shared_ptr<GameSession>> game =
      manager.CreateGame(json::parse(config));
  CHECK_OK(game.status());
  return *game;
}

// Waits for the game to finish, failing the test after a while.
json WaitUntilFinished(GameSession& game) {
  const absl::Time deadline = absl::Now() + absl::Seconds(30);
  json state = game.State();
  while (!state["finished"].get<bool>() && absl::Now() < deadline) {
    state = game.WaitForState(state["version"].get<int>(), absl::Seconds(1));
  }
  EXPECT_THAT(state["finished"], Eq(true));
  return state;
}

TEST(GameManagerTest, CreateRejectsBadConfigs) {
  GameManager manager(TestOptions());
  EXPECT_THAT(manager.CreateGame(json::parse(R"({"num_players": 3})"))
                  .status()
                  .code(),
              Eq(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(manager.ListGames(), SizeIs(0));
}

TEST(GameManagerTest, GetGame) {
  GameManager manager(TestOptions());
  const std::shared_ptr<GameSession> game = Create(manager, kHumanVsAi);
  EXPECT_THAT(manager.GetGame(game->id()), Eq(game));
  EXPECT_THAT(manager.GetGame("nonexistent"), IsNull());
}

TEST(GameManagerTest, AiRepliesToHumanMoves) {
  GameManager manager(TestOptions());
  const std::shared_ptr<GameSession> game = Create(manager, kHumanVsAi);
  const json legal = game->LegalMoves();
  ASSERT_TRUE(game->PlayMove(0, legal["moves"][0]).ok());
  const json state = game->WaitForState(1, absl::Seconds(30));
  EXPECT_THAT(state["version"], Eq(2));
  EXPECT_THAT(state["moves"], SizeIs(2));
  EXPECT_THAT(state["current_color"], Eq("red"));
}

// AI-only games play themselves, taking turns on the AI thread.
TEST(GameManagerTest, AiGamesPlayToTheEnd) {
  GameManager manager(TestOptions());
  const std::shared_ptr<GameSession> first = Create(manager, kAiVsAi);
  const std::shared_ptr<GameSession> second = Create(manager, kAiVsAi);
  WaitUntilFinished(*first);
  WaitUntilFinished(*second);
}

TEST(GameManagerTest, ListGamesMostRecentFirst) {
  GameManager manager(TestOptions());
  const std::shared_ptr<GameSession> first = Create(manager, kHumanVsAi);
  absl::SleepFor(absl::Milliseconds(5));
  const std::shared_ptr<GameSession> second = Create(manager, kHumanVsAi);
  json games = manager.ListGames();
  ASSERT_THAT(games, SizeIs(2));
  EXPECT_THAT(games[0]["id"], Eq(second->id()));
  EXPECT_THAT(games[1]["id"], Eq(first->id()));
  EXPECT_THAT(games[0]["num_moves"], Eq(0));
  EXPECT_THAT(games[0]["seats"][1]["preset"], Eq("low"));

  absl::SleepFor(absl::Milliseconds(5));
  ASSERT_TRUE(first->PlayMove(0, first->LegalMoves()["moves"][0]).ok());
  first->WaitForState(1, absl::Seconds(30));
  games = manager.ListGames();
  EXPECT_THAT(games[0]["id"], Eq(first->id()));
}

TEST(GameManagerTest, DeleteGame) {
  GameManager manager(TestOptions());
  const std::shared_ptr<GameSession> game = Create(manager, kHumanVsAi);
  ASSERT_TRUE(manager.DeleteGame(game->id()).ok());
  EXPECT_THAT(manager.GetGame(game->id()), IsNull());
  EXPECT_THAT(manager.ListGames(), SizeIs(0));
  EXPECT_THAT(manager.DeleteGame(game->id()).code(),
              Eq(absl::StatusCode::kNotFound));
}

TEST(GameManagerTest, GamesSurviveARestart) {
  const std::string dir = MakeDataDir("restart");
  std::string id;
  json before;
  {
    GameManager manager(TestOptions(dir));
    const std::shared_ptr<GameSession> game = Create(manager, kHumanVsAi);
    id = game->id();
    for (int i = 0; i < 3; ++i) {
      const json legal = game->LegalMoves();
      const int version = legal["version"].get<int>();
      ASSERT_TRUE(game->PlayMove(version, legal["moves"][0]).ok());
      game->WaitForState(version + 1, absl::Seconds(30));
    }
    before = game->State();
  }
  GameManager manager(TestOptions(dir));
  const std::shared_ptr<GameSession> game = manager.GetGame(id);
  ASSERT_THAT(game, NotNull());
  json after = game->State();
  before.erase("version");
  after.erase("version");
  EXPECT_THAT(after, Eq(before));
}

// A game saved during an AI's turn continues after a restart.
TEST(GameManagerTest, AiGameResumesAfterARestart) {
  const std::string dir = MakeDataDir("resume");
  Gate gate;
  std::string id;
  std::thread opener;
  {
    GameManager manager({.data_dir = dir,
                         .ai_factory = FirstMoveAiFactory(&gate)});
    id = Create(manager, kAiVsAi)->id();
    // Shut down during the first search. The manager waits for it to end,
    // and discards its move.
    gate.WaitForAi();
    opener = std::thread([&] {
      absl::SleepFor(absl::Milliseconds(20));
      gate.Open();
    });
  }
  opener.join();
  GameManager manager(TestOptions(dir));
  const std::shared_ptr<GameSession> game = manager.GetGame(id);
  ASSERT_THAT(game, NotNull());
  WaitUntilFinished(*game);
}

TEST(GameManagerTest, DeleteRemovesTheSave) {
  const std::string dir = MakeDataDir("delete");
  GameManager manager(TestOptions(dir));
  const std::shared_ptr<GameSession> game = Create(manager, kHumanVsAi);
  const std::filesystem::path save =
      std::filesystem::path(dir) / (game->id() + ".json");
  EXPECT_TRUE(std::filesystem::exists(save));
  ASSERT_TRUE(manager.DeleteGame(game->id()).ok());
  EXPECT_FALSE(std::filesystem::exists(save));
}

TEST(GameManagerTest, SkipsBadSaves) {
  const std::string dir = MakeDataDir("bad_saves");
  std::string id;
  {
    GameManager manager(TestOptions(dir));
    id = Create(manager, kHumanVsAi)->id();
  }
  std::ofstream(std::filesystem::path(dir) / "garbage.json") << "{not json";
  std::ofstream(std::filesystem::path(dir) / "wrong_id.json")
      << R"({"id": "other"})";
  std::ofstream(std::filesystem::path(dir) / "notes.txt") << "hello";
  GameManager manager(TestOptions(dir));
  EXPECT_THAT(manager.ListGames(), SizeIs(1));
  EXPECT_THAT(manager.GetGame(id), NotNull());
}

}  // namespace
}  // namespace blokus
