#include "server/json.h"

#include <algorithm>
#include <random>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "game/board.h"
#include "game/game.h"
#include "game/tile.h"

namespace blokus {
namespace {

using ::nlohmann::json;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::SizeIs;

Move MakeTestMove(Color color, int tile, int row, int col) {
  Move move;
  move.color = color;
  move.tile = tile;
  move.placement = {Coord(row, col), 0, false};
  return move;
}

// Plays the first possible move, or passes, until the game is finished.
void PlayToEnd(Game& game) {
  while (!game.Finished()) {
    const std::vector<Move> moves = game.PossibleMoves();
    ASSERT_TRUE(game.MakeMove(moves.empty()
                                  ? Move::EmptyMove(game.current_color())
                                  : moves.front()));
  }
}

absl::StatusCode MoveFromJsonCode(const json& move_json, const Game& game) {
  return MoveFromJson(move_json, game).status().code();
}

TEST(PlayerForColorTest, TwoPlayers) {
  EXPECT_THAT(PlayerForColor(2, BLUE), Eq(0));
  EXPECT_THAT(PlayerForColor(2, YELLOW), Eq(1));
  EXPECT_THAT(PlayerForColor(2, RED), Eq(0));
  EXPECT_THAT(PlayerForColor(2, GREEN), Eq(1));
}

TEST(PlayerForColorTest, FourPlayers) {
  EXPECT_THAT(PlayerForColor(4, BLUE), Eq(0));
  EXPECT_THAT(PlayerForColor(4, YELLOW), Eq(1));
  EXPECT_THAT(PlayerForColor(4, RED), Eq(2));
  EXPECT_THAT(PlayerForColor(4, GREEN), Eq(3));
}

TEST(TilesToJsonTest, OneEntryPerTile) {
  const json tiles = TilesToJson();
  ASSERT_THAT(tiles, SizeIs(kNumTiles));
  for (int tile = 0; tile < kNumTiles; ++tile) {
    EXPECT_THAT(tiles[tile]["tile"], Eq(tile));
    EXPECT_THAT(tiles[tile]["size"], Eq(kTiles[tile].Size()));
    EXPECT_THAT(tiles[tile]["orientations"],
                SizeIs(kTiles[tile].orientations().size()));
  }
}

TEST(TilesToJsonTest, SymmetricTiles) {
  const json tiles = TilesToJson();
  // The 1x1 looks the same however it's turned.
  EXPECT_THAT(tiles[0]["orientations"], SizeIs(1));
  EXPECT_THAT(tiles[0]["transforms"],
              Eq(json::parse("[[0, 0], [0, 0], [0, 0], [0, 0]]")));
  // The 1x5 has a horizontal and a vertical orientation.
  EXPECT_THAT(tiles[14]["orientations"], SizeIs(2));
}

TEST(TilesToJsonTest, OrientationCellsStartAtZero) {
  for (const json& tile : TilesToJson()) {
    for (const json& orientation : tile["orientations"]) {
      ASSERT_THAT(orientation["cells"], SizeIs(tile["size"].get<int>()));
      int min_row = 100;
      int min_col = 100;
      for (const json& cell : orientation["cells"]) {
        min_row = std::min(min_row, cell[0].get<int>());
        min_col = std::min(min_col, cell[1].get<int>());
      }
      EXPECT_THAT(min_row, Eq(0)) << tile["tile"];
      EXPECT_THAT(min_col, Eq(0)) << tile["tile"];
    }
  }
}

// Every transform is a valid orientation, and together they reach all of
// them.
TEST(TilesToJsonTest, TransformsCoverAllOrientations) {
  for (const json& tile : TilesToJson()) {
    const int num_orientations = tile["orientations"].size();
    std::vector<bool> reached(num_orientations, false);
    ASSERT_THAT(tile["transforms"], SizeIs(4));
    for (const json& by_flip : tile["transforms"]) {
      ASSERT_THAT(by_flip, SizeIs(2));
      for (const json& index : by_flip) {
        ASSERT_THAT(index.get<int>(), Ge(0));
        ASSERT_THAT(index.get<int>(), testing::Lt(num_orientations));
        reached[index.get<int>()] = true;
      }
    }
    EXPECT_THAT(std::count(reached.begin(), reached.end(), true),
                Eq(num_orientations))
        << tile["tile"];
  }
}

TEST(MoveToJsonTest, Pass) {
  EXPECT_THAT(MoveToJson(Move::EmptyMove(RED)),
              Eq(json::parse(R"({"color": "red", "pass": true})")));
}

TEST(MoveToJsonTest, TileMove) {
  // The 2x1 at the origin covers (0, 0) and (0, 1).
  EXPECT_THAT(
      MoveToJson(MakeTestMove(BLUE, 1, 0, 0)),
      Eq(json::parse(R"({"color": "blue", "tile": 1,
                         "cells": [[0, 0], [0, 1]]})")));
}

TEST(GameToJsonTest, InitialState) {
  const json state = GameToJson(Game(2));
  EXPECT_THAT(state["num_players"], Eq(2));
  EXPECT_THAT(state["finished"], Eq(false));
  EXPECT_THAT(state["current_color"], Eq("blue"));
  EXPECT_THAT(state["current_player"], Eq(0));
  ASSERT_THAT(state["board"], SizeIs(Board::kNumRows));
  for (const json& row : state["board"]) {
    ASSERT_THAT(row, SizeIs(Board::kNumCols));
    for (const json& cell : row) EXPECT_TRUE(cell.is_null());
  }
  EXPECT_THAT(state["colors"]["blue"]["player"], Eq(0));
  EXPECT_THAT(state["colors"]["yellow"]["player"], Eq(1));
  EXPECT_THAT(state["colors"]["red"]["player"], Eq(0));
  EXPECT_THAT(state["colors"]["green"]["player"], Eq(1));
  EXPECT_THAT(state["colors"]["blue"]["score"], Eq(-89));
  EXPECT_THAT(state["colors"]["blue"]["passed"], Eq(false));
  EXPECT_THAT(state["colors"]["blue"]["unplayed_tiles"], SizeIs(kNumTiles));
  EXPECT_THAT(state["scores"], Eq(json::parse("[-178, -178]")));
  EXPECT_THAT(state["moves"], SizeIs(0));
  EXPECT_FALSE(state.contains("winners"));
}

TEST(GameToJsonTest, AfterMoves) {
  Game game(4);
  ASSERT_TRUE(game.MakeMove(MakeTestMove(BLUE, 1, 0, 0)));
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(YELLOW)));
  const json state = GameToJson(game);
  EXPECT_THAT(state["current_color"], Eq("red"));
  EXPECT_THAT(state["current_player"], Eq(2));
  EXPECT_THAT(state["board"][0][0], Eq("blue"));
  EXPECT_THAT(state["board"][0][1], Eq("blue"));
  EXPECT_TRUE(state["board"][0][2].is_null());
  EXPECT_THAT(state["colors"]["blue"]["score"], Eq(-87));
  EXPECT_THAT(state["colors"]["blue"]["unplayed_tiles"][0], Eq(0));
  EXPECT_THAT(state["colors"]["blue"]["unplayed_tiles"][1], Eq(2));
  EXPECT_THAT(state["colors"]["yellow"]["passed"], Eq(true));
  EXPECT_THAT(state["scores"], Eq(json::parse("[-87, -89, -89, -89]")));
  ASSERT_THAT(state["moves"], SizeIs(2));
  EXPECT_THAT(state["moves"][0], Eq(MoveToJson(game.moves()[0])));
  EXPECT_THAT(state["moves"][1], Eq(MoveToJson(game.moves()[1])));
}

TEST(GameToJsonTest, FinishedGame) {
  Game game(2);
  PlayToEnd(game);
  const json state = GameToJson(game);
  EXPECT_THAT(state["finished"], Eq(true));
  EXPECT_FALSE(state.contains("current_color"));
  EXPECT_FALSE(state.contains("current_player"));
  const GameResult result = game.Result();
  EXPECT_THAT(state["scores"], Eq(json(result.scores)));
  ASSERT_THAT(state["winners"], SizeIs(Ge(1)));
  for (const json& winner : state["winners"]) {
    EXPECT_THAT(result.scores[winner.get<int>()],
                Eq(*std::max_element(result.scores.begin(),
                                     result.scores.end())));
  }
}

TEST(GameToJsonTest, TiedWinners) {
  Game game(4);
  while (!game.Finished()) {
    ASSERT_TRUE(game.MakeMove(Move::EmptyMove(game.current_color())));
  }
  EXPECT_THAT(GameToJson(game)["winners"],
              Eq(json::parse("[0, 1, 2, 3]")));
}

TEST(PossibleMovesToJsonTest, MatchesPossibleMoves) {
  Game game(4);
  const std::vector<Move> moves = game.PossibleMoves();
  const json moves_json = PossibleMovesToJson(game);
  ASSERT_THAT(moves_json, SizeIs(moves.size()));
  for (int i = 0; i < static_cast<int>(moves.size()); ++i) {
    EXPECT_THAT(moves_json[i], Eq(MoveToJson(moves[i])));
  }
}

TEST(PossibleMovesToJsonTest, EmptyWhenFinished) {
  Game game(2);
  PlayToEnd(game);
  EXPECT_THAT(PossibleMovesToJson(game), SizeIs(0));
}

// Every possible move survives a round trip, even with its cells shuffled.
TEST(MoveFromJsonTest, RoundTripsPossibleMoves) {
  Game game(2);
  std::mt19937_64 rng(1);
  for (int turn = 0; turn < 8; ++turn) {
    const std::vector<Move> moves = game.PossibleMoves();
    ASSERT_THAT(moves, SizeIs(Gt(0)));
    for (const Move& move : moves) {
      json move_json = MoveToJson(move);
      std::shuffle(move_json["cells"].begin(), move_json["cells"].end(), rng);
      const absl::StatusOr<Move> parsed = MoveFromJson(move_json, game);
      ASSERT_TRUE(parsed.ok()) << parsed.status();
      EXPECT_THAT(*parsed, Eq(move));
    }
    ASSERT_TRUE(game.MakeMove(moves[moves.size() / 2]));
  }
}

TEST(MoveFromJsonTest, RejectsMalformedMoves) {
  const Game game(4);
  for (const char* bad : {
           R"([1, 2])",
           R"({})",
           R"({"tile": "1", "cells": [[0, 0], [0, 1]]})",
           R"({"tile": 1})",
           R"({"tile": 1, "cells": "none"})",
           R"({"tile": 1, "cells": [[0, 0], [0]]})",
           R"({"tile": 1, "cells": [[0, 0], [0, 1.5]]})",
           R"({"tile": 21, "cells": [[0, 0]]})",
           R"({"tile": -1, "cells": []})",
           R"({"pass": false})",
       }) {
    EXPECT_THAT(MoveFromJsonCode(json::parse(bad), game),
                Eq(absl::StatusCode::kInvalidArgument))
        << bad;
  }
}

TEST(MoveFromJsonTest, RejectsIllegalMoves) {
  const Game game(4);
  // Not in BLUE's corner.
  EXPECT_THAT(MoveFromJsonCode(
                  json::parse(R"({"tile": 1, "cells": [[5, 5], [5, 6]]})"),
                  game),
              Eq(absl::StatusCode::kInvalidArgument));
  // The cells don't match the tile.
  EXPECT_THAT(MoveFromJsonCode(
                  json::parse(R"({"tile": 0, "cells": [[0, 0], [0, 1]]})"),
                  game),
              Eq(absl::StatusCode::kInvalidArgument));
}

TEST(MoveFromJsonTest, PassOnlyWithoutMoves) {
  Game game(4);
  const json pass = json::parse(R"({"pass": true})");
  EXPECT_THAT(MoveFromJsonCode(pass, game),
              Eq(absl::StatusCode::kInvalidArgument));
  // Once BLUE has passed, it has no moves, so it can pass again.
  ASSERT_TRUE(game.MakeMove(Move::EmptyMove(BLUE)));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(YELLOW, 0, 0, 19)));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(RED, 0, 19, 19)));
  ASSERT_TRUE(game.MakeMove(MakeTestMove(GREEN, 0, 19, 0)));
  const absl::StatusOr<Move> parsed = MoveFromJson(pass, game);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_THAT(*parsed, Eq(Move::EmptyMove(BLUE)));
}

TEST(MoveFromJsonTest, RejectsMovesOnceFinished) {
  Game game(2);
  PlayToEnd(game);
  EXPECT_THAT(MoveFromJsonCode(json::parse(R"({"pass": true})"), game),
              Eq(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace blokus
