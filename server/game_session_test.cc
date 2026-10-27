#include "server/game_session.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "game/board.h"
#include "server/test_ai.h"

namespace blokus {
namespace {

using ::nlohmann::json;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::SizeIs;

// A config with a human or AI in each seat.
GameConfig MakeConfig(const std::vector<bool>& human) {
  GameConfig config;
  config.num_players = human.size();
  for (bool is_human : human) {
    SeatConfig seat;
    seat.human = is_human;
    seat.preset = is_human ? "" : "custom";
    config.seats.push_back(seat);
  }
  return config;
}

std::unique_ptr<GameSession> MakeSession(const std::vector<bool>& human,
                                         Gate* gate = nullptr,
                                         std::string save_path = "") {
  absl::StatusOr<std::unique_ptr<GameSession>> session =
      GameSession::Create("test", MakeConfig(human), {},
                          FirstMoveAiFactory(gate), std::move(save_path));
  CHECK_OK(session.status());
  return *std::move(session);
}

int Version(const GameSession& session) {
  return session.State()["version"].get<int>();
}

int NumMoves(const GameSession& session) {
  return session.State()["moves"].size();
}

// Plays the human's first legal move.
absl::Status PlayFirstMove(GameSession& session) {
  const json legal = session.LegalMoves();
  return session.PlayMove(legal["version"].get<int>(), legal["moves"][0]);
}

std::string TempPath(const std::string& name) {
  return (std::filesystem::path(::testing::TempDir()) / name).string();
}

TEST(GameSessionTest, InitialState) {
  const std::unique_ptr<GameSession> session = MakeSession({true, false});
  const json state = session->State();
  EXPECT_THAT(state["id"], Eq("test"));
  EXPECT_THAT(state["version"], Eq(0));
  EXPECT_THAT(state["num_players"], Eq(2));
  EXPECT_THAT(state["seats"][0]["type"], Eq("human"));
  EXPECT_THAT(state["seats"][1]["type"], Eq("ai"));
  EXPECT_THAT(state["ai_thinking"], Eq(false));
  EXPECT_THAT(state["can_undo"], Eq(false));
  EXPECT_FALSE(session->NeedsAiTurn());
}

TEST(GameSessionTest, HumansPlayMoves) {
  const std::unique_ptr<GameSession> session = MakeSession({true, true});
  const json legal = session->LegalMoves();
  EXPECT_THAT(legal["version"], Eq(0));
  ASSERT_THAT(legal["moves"], SizeIs(Gt(0)));
  ASSERT_TRUE(session->PlayMove(0, legal["moves"][0]).ok());
  const json state = session->State();
  EXPECT_THAT(state["version"], Eq(1));
  ASSERT_THAT(state["moves"], SizeIs(1));
  EXPECT_THAT(state["moves"][0], Eq(legal["moves"][0]));
  EXPECT_THAT(state["current_color"], Eq("yellow"));
  EXPECT_THAT(state["can_undo"], Eq(true));
}

TEST(GameSessionTest, RejectsStaleVersion) {
  const std::unique_ptr<GameSession> session = MakeSession({true, true});
  const json move = session->LegalMoves()["moves"][0];
  EXPECT_THAT(session->PlayMove(1, move).code(),
              Eq(absl::StatusCode::kFailedPrecondition));
  ASSERT_TRUE(session->PlayMove(0, move).ok());
  // Replaying the same request, e.g. after a double-click, does nothing.
  EXPECT_THAT(session->PlayMove(0, move).code(),
              Eq(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(NumMoves(*session), Eq(1));
}

TEST(GameSessionTest, RejectsIllegalMoves) {
  const std::unique_ptr<GameSession> session = MakeSession({true, true});
  EXPECT_THAT(
      session->PlayMove(0, json::parse(R"({"tile": 0, "cells": [[5, 5]]})"))
          .code(),
      Eq(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(session->PlayMove(0, json::parse(R"({"pass": true})")).code(),
              Eq(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Version(*session), Eq(0));
}

TEST(GameSessionTest, AiTakesItsTurn) {
  const std::unique_ptr<GameSession> session = MakeSession({true, false});
  ASSERT_TRUE(PlayFirstMove(*session).ok());
  EXPECT_TRUE(session->NeedsAiTurn());
  EXPECT_THAT(session->State()["ai_thinking"], Eq(true));
  // The human can't move for the AI.
  EXPECT_THAT(PlayFirstMove(*session).code(),
              Eq(absl::StatusCode::kFailedPrecondition));
  session->RunAiTurn();
  const json state = session->State();
  EXPECT_THAT(state["version"], Eq(2));
  EXPECT_THAT(state["moves"], SizeIs(2));
  EXPECT_THAT(state["moves"][1]["color"], Eq("yellow"));
  EXPECT_THAT(state["ai_thinking"], Eq(false));
  EXPECT_FALSE(session->NeedsAiTurn());
}

TEST(GameSessionTest, RunAiTurnDoesNothingOnHumanTurn) {
  const std::unique_ptr<GameSession> session = MakeSession({true, false});
  session->RunAiTurn();
  EXPECT_THAT(Version(*session), Eq(0));
}

TEST(GameSessionTest, AisPlayToTheEnd) {
  const std::unique_ptr<GameSession> session =
      MakeSession({false, false, false, false});
  int turns = 0;
  while (session->NeedsAiTurn()) {
    session->RunAiTurn();
    ASSERT_THAT(++turns, testing::Lt(200));
  }
  const json state = session->State();
  EXPECT_THAT(state["finished"], Eq(true));
  EXPECT_THAT(state["winners"], SizeIs(Ge(1)));
  EXPECT_THAT(state["ai_thinking"], Eq(false));
}

// A color with no moves passes automatically. Here BLUE passed at the start,
// which the engine allows, so it can only pass from then on.
TEST(GameSessionTest, ForcedPassesAreAutomatic) {
  Move pass = Move::EmptyMove(BLUE);
  Move yellow, red, green;
  yellow.color = YELLOW;
  yellow.tile = 0;
  yellow.placement.coord = Coord(0, 19);
  red = yellow;
  red.color = RED;
  red.placement.coord = Coord(19, 19);
  green = yellow;
  green.color = GREEN;
  green.placement.coord = Coord(19, 0);
  absl::StatusOr<std::unique_ptr<GameSession>> session = GameSession::Create(
      "test", MakeConfig({true, true, true, true}),
      {pass, yellow, red, green}, FirstMoveAiFactory());
  ASSERT_TRUE(session.ok()) << session.status();
  const json state = (*session)->State();
  ASSERT_THAT(state["moves"], SizeIs(5));
  EXPECT_THAT(state["moves"][4], Eq(json::parse(
                                     R"({"color": "blue", "pass": true})")));
  EXPECT_THAT(state["current_color"], Eq("yellow"));
}

TEST(GameSessionTest, CreateRejectsIllegalMoves) {
  Move move;
  move.color = BLUE;
  move.tile = 0;
  move.placement.coord = Coord(5, 5);
  EXPECT_THAT(GameSession::Create("test", MakeConfig({true, true}), {move},
                                  FirstMoveAiFactory())
                  .status()
                  .code(),
              Eq(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(GameSession::Create("test", MakeConfig({true}), {},
                                  FirstMoveAiFactory())
                  .status()
                  .code(),
              Eq(absl::StatusCode::kInvalidArgument));
}

TEST(GameSessionTest, UndoTakesBackTheHumanMoveAndAiReply) {
  const std::unique_ptr<GameSession> session = MakeSession({true, false});
  ASSERT_TRUE(PlayFirstMove(*session).ok());
  session->RunAiTurn();
  ASSERT_THAT(NumMoves(*session), Eq(2));
  EXPECT_THAT(session->Undo(1).code(),
              Eq(absl::StatusCode::kFailedPrecondition));
  ASSERT_TRUE(session->Undo(2).ok());
  const json state = session->State();
  EXPECT_THAT(state["version"], Eq(3));
  EXPECT_THAT(state["moves"], SizeIs(0));
  EXPECT_THAT(state["current_color"], Eq("blue"));
  EXPECT_THAT(state["can_undo"], Eq(false));
  EXPECT_THAT(session->Undo(3).code(),
              Eq(absl::StatusCode::kFailedPrecondition));
}

TEST(GameSessionTest, RepeatedUndo) {
  const std::unique_ptr<GameSession> session = MakeSession({true, false});
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(PlayFirstMove(*session).ok());
    session->RunAiTurn();
  }
  ASSERT_THAT(NumMoves(*session), Eq(6));
  ASSERT_TRUE(session->Undo(Version(*session)).ok());
  EXPECT_THAT(NumMoves(*session), Eq(4));
  ASSERT_TRUE(session->Undo(Version(*session)).ok());
  EXPECT_THAT(NumMoves(*session), Eq(2));
  // The AI plays again from the restored position.
  ASSERT_TRUE(PlayFirstMove(*session).ok());
  session->RunAiTurn();
  EXPECT_THAT(NumMoves(*session), Eq(4));
}

// With several humans, undo goes back to the last human move, whoever made
// it.
TEST(GameSessionTest, UndoStopsAtTheLastHumanMove) {
  const std::unique_ptr<GameSession> session =
      MakeSession({true, true, false, false});
  ASSERT_TRUE(PlayFirstMove(*session).ok());  // BLUE, human.
  ASSERT_TRUE(PlayFirstMove(*session).ok());  // YELLOW, human.
  session->RunAiTurn();                       // RED.
  session->RunAiTurn();                       // GREEN.
  ASSERT_THAT(NumMoves(*session), Eq(4));
  ASSERT_TRUE(session->Undo(Version(*session)).ok());
  EXPECT_THAT(NumMoves(*session), Eq(1));
  EXPECT_THAT(session->State()["current_color"], Eq("yellow"));
}

TEST(GameSessionTest, NoUndoWithoutHumans) {
  const std::unique_ptr<GameSession> session = MakeSession({false, false});
  session->RunAiTurn();
  EXPECT_THAT(session->State()["can_undo"], Eq(false));
  EXPECT_THAT(session->Undo(Version(*session)).code(),
              Eq(absl::StatusCode::kFailedPrecondition));
}

// An undo during an AI's search discards the search's move.
TEST(GameSessionTest, UndoDiscardsAiSearch) {
  Gate gate;
  const std::unique_ptr<GameSession> session =
      MakeSession({true, false}, &gate);
  ASSERT_TRUE(PlayFirstMove(*session).ok());
  std::thread ai([&] { session->RunAiTurn(); });
  gate.WaitForAi();
  ASSERT_TRUE(session->Undo(1).ok());
  gate.Open();
  ai.join();
  const json state = session->State();
  EXPECT_THAT(state["moves"], SizeIs(0));
  EXPECT_THAT(state["version"], Eq(2));
}

TEST(GameSessionTest, WaitForStateReturnsNewerVersionsImmediately) {
  const std::unique_ptr<GameSession> session = MakeSession({true, true});
  ASSERT_TRUE(PlayFirstMove(*session).ok());
  EXPECT_THAT(session->WaitForState(0, absl::Hours(1))["version"], Eq(1));
}

TEST(GameSessionTest, WaitForStateTimesOut) {
  const std::unique_ptr<GameSession> session = MakeSession({true, true});
  const absl::Time start = absl::Now();
  EXPECT_THAT(session->WaitForState(0, absl::Milliseconds(50))["version"],
              Eq(0));
  EXPECT_THAT(absl::Now() - start, Ge(absl::Milliseconds(50)));
}

TEST(GameSessionTest, WaitForStateWakesOnMove) {
  const std::unique_ptr<GameSession> session = MakeSession({true, true});
  json state;
  std::thread waiter(
      [&] { state = session->WaitForState(0, absl::Seconds(30)); });
  absl::SleepFor(absl::Milliseconds(20));
  ASSERT_TRUE(PlayFirstMove(*session).ok());
  waiter.join();
  EXPECT_THAT(state["version"], Eq(1));
}

TEST(GameSessionTest, CloseWakesWaitersAndStopsAis) {
  const std::unique_ptr<GameSession> session = MakeSession({false, true});
  std::thread waiter(
      [&] { session->WaitForState(0, absl::Seconds(30)); });
  session->Close();
  waiter.join();
  EXPECT_FALSE(session->NeedsAiTurn());
  session->RunAiTurn();
  EXPECT_THAT(NumMoves(*session), Eq(0));
  EXPECT_THAT(PlayFirstMove(*session).code(),
              Eq(absl::StatusCode::kFailedPrecondition));
}

TEST(GameSessionTest, SavesAndLoads) {
  const std::string path = TempPath("saves_and_loads.json");
  const std::unique_ptr<GameSession> session =
      MakeSession({true, false}, nullptr, path);
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(PlayFirstMove(*session).ok());
    session->RunAiTurn();
  }
  std::ifstream file(path);
  const json saved = json::parse(file);
  EXPECT_THAT(saved, Eq(session->SaveJson()));

  absl::StatusOr<std::unique_ptr<GameSession>> loaded =
      GameSession::Load(saved, FirstMoveAiFactory());
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  json before = session->State();
  json after = (*loaded)->State();
  // The version restarts, so clients refetch the state.
  EXPECT_THAT(after["version"], Eq(0));
  before.erase("version");
  after.erase("version");
  EXPECT_THAT(after, Eq(before));
  EXPECT_THAT((*loaded)->Summary()["updated"],
              Eq(session->Summary()["updated"]));
}

TEST(GameSessionTest, LoadedGameContinues) {
  const std::unique_ptr<GameSession> session = MakeSession({true, false});
  ASSERT_TRUE(PlayFirstMove(*session).ok());
  // Saved mid-turn, while the AI was due to move.
  absl::StatusOr<std::unique_ptr<GameSession>> loaded =
      GameSession::Load(session->SaveJson(), FirstMoveAiFactory());
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_TRUE((*loaded)->NeedsAiTurn());
  (*loaded)->RunAiTurn();
  EXPECT_THAT(NumMoves(**loaded), Eq(2));
  ASSERT_TRUE((*loaded)->Undo(Version(**loaded)).ok());
  EXPECT_THAT(NumMoves(**loaded), Eq(0));
}

TEST(GameSessionTest, LoadRejectsBadSaves) {
  const std::unique_ptr<GameSession> session = MakeSession({true, true});
  ASSERT_TRUE(PlayFirstMove(*session).ok());
  const json good = session->SaveJson();

  json no_moves = good;
  no_moves.erase("moves");
  json illegal = good;
  illegal["moves"][0]["cells"] = json::parse("[[9, 9]]");
  json bad_config = good;
  bad_config["config"]["num_players"] = 3;
  json bad_time = good;
  bad_time["updated"] = "yesterday";
  for (const json& bad :
       {json::parse("[]"), no_moves, illegal, bad_config, bad_time}) {
    EXPECT_FALSE(GameSession::Load(bad, FirstMoveAiFactory()).ok()) << bad;
  }
}

}  // namespace
}  // namespace blokus
