#include "server/config.h"

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace blokus {
namespace {

using ::nlohmann::json;
using ::testing::DoubleEq;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::SizeIs;

absl::StatusCode ParseCode(const char* config) {
  return GameConfigFromJson(json::parse(config)).status().code();
}

TEST(PresetsTest, IncreasingStrength) {
  const json presets = PresetsToJson();
  EXPECT_THAT(presets["low"]["num_iterations"], Eq(300));
  EXPECT_THAT(presets["medium"]["num_iterations"], Eq(3000));
  EXPECT_THAT(presets["high"]["num_iterations"], Eq(100000));
  EXPECT_THAT(presets["low"]["num_threads"], Eq(1));
  EXPECT_THAT(presets["high"]["num_threads"].get<int>(), Ge(1));
}

TEST(GameConfigTest, HumanAndPresetSeats) {
  const absl::StatusOr<GameConfig> config = GameConfigFromJson(json::parse(R"(
      {"num_players": 2,
       "seats": [{"type": "human"}, {"type": "ai", "preset": "medium"}]})"));
  ASSERT_TRUE(config.ok()) << config.status();
  EXPECT_THAT(config->num_players, Eq(2));
  ASSERT_THAT(config->seats, SizeIs(2));
  EXPECT_TRUE(config->seats[0].human);
  EXPECT_FALSE(config->seats[1].human);
  EXPECT_THAT(config->seats[1].preset, Eq("medium"));
  EXPECT_THAT(config->seats[1].mcts.num_iterations, Eq(3000));
}

TEST(GameConfigTest, CustomSeat) {
  const absl::StatusOr<GameConfig> config = GameConfigFromJson(json::parse(R"(
      {"num_players": 4,
       "seats": [{"type": "human"}, {"type": "human"}, {"type": "human"},
                 {"type": "ai", "mcts": {"c": 0.5, "num_iterations": 1234,
                                         "num_rollouts_per_iteration": 2,
                                         "num_threads": 3,
                                         "virtual_loss": 1}}]})"));
  ASSERT_TRUE(config.ok()) << config.status();
  const SeatConfig& seat = config->seats[3];
  EXPECT_THAT(seat.preset, Eq("custom"));
  EXPECT_THAT(seat.mcts.c, DoubleEq(0.5));
  EXPECT_THAT(seat.mcts.num_iterations, Eq(1234));
  EXPECT_THAT(seat.mcts.num_rollouts_per_iteration, Eq(2));
  EXPECT_THAT(seat.mcts.num_threads, Eq(3));
  EXPECT_THAT(seat.mcts.virtual_loss, Eq(1));
}

TEST(GameConfigTest, CustomSeatKeepsDefaults) {
  const absl::StatusOr<GameConfig> config = GameConfigFromJson(json::parse(R"(
      {"num_players": 2,
       "seats": [{"type": "human"},
                 {"type": "ai", "mcts": {"num_iterations": 50}}]})"));
  ASSERT_TRUE(config.ok()) << config.status();
  const MctsOptions defaults;
  EXPECT_THAT(config->seats[1].mcts.num_iterations, Eq(50));
  EXPECT_THAT(config->seats[1].mcts.c, DoubleEq(defaults.c));
  EXPECT_THAT(config->seats[1].mcts.virtual_loss, Eq(defaults.virtual_loss));
}

TEST(GameConfigTest, RoundTrips) {
  const json original = json::parse(R"(
      {"num_players": 4,
       "seats": [{"type": "human"},
                 {"type": "ai", "preset": "low"},
                 {"type": "ai", "preset": "high"},
                 {"type": "ai", "mcts": {"c": 0.7, "num_iterations": 77}}]})");
  const absl::StatusOr<GameConfig> config = GameConfigFromJson(original);
  ASSERT_TRUE(config.ok()) << config.status();
  const json written = GameConfigToJson(*config);
  EXPECT_THAT(written["seats"][1]["preset"], Eq("low"));
  EXPECT_THAT(written["seats"][3]["preset"], Eq("custom"));
  EXPECT_THAT(written["seats"][3]["mcts"]["num_iterations"], Eq(77));
  const absl::StatusOr<GameConfig> reread = GameConfigFromJson(written);
  ASSERT_TRUE(reread.ok()) << reread.status();
  EXPECT_THAT(GameConfigToJson(*reread), Eq(written));
}

TEST(GameConfigTest, RejectsBadConfigs) {
  for (const char* bad : {
           R"([])",
           R"({"seats": [{"type": "human"}, {"type": "human"}]})",
           R"({"num_players": 3,
               "seats": [{"type": "human"}, {"type": "human"},
                         {"type": "human"}]})",
           R"({"num_players": 2, "seats": [{"type": "human"}]})",
           R"({"num_players": 2, "seats": "human"})",
           R"({"num_players": 2, "seats": [{"type": "human"}, {}]})",
           R"({"num_players": 2,
               "seats": [{"type": "human"}, {"type": "robot"}]})",
           R"({"num_players": 2,
               "seats": [{"type": "human"}, {"type": "ai"}]})",
           R"({"num_players": 2,
               "seats": [{"type": "human"},
                         {"type": "ai", "preset": "extreme"}]})",
           R"({"num_players": 2,
               "seats": [{"type": "human"}, {"type": "ai", "preset": 1}]})",
           R"({"num_players": 2,
               "seats": [{"type": "human"}, {"type": "ai", "mcts": 5}]})",
       }) {
    EXPECT_THAT(ParseCode(bad), Eq(absl::StatusCode::kInvalidArgument))
        << bad;
  }
}

TEST(GameConfigTest, RejectsBadMctsOptions) {
  for (const char* mcts : {
           R"({"c": -1})",
           R"({"c": "big"})",
           R"({"num_iterations": 0})",
           R"({"num_iterations": 1.5})",
           R"({"num_iterations": 100000000})",
           R"({"num_rollouts_per_iteration": 0})",
           R"({"num_threads": 0})",
           R"({"num_threads": 1000})",
           R"({"virtual_loss": -1})",
           R"({"seed": 3})",
       }) {
    json config = json::parse(R"({"num_players": 2,
        "seats": [{"type": "human"}, {"type": "ai"}]})");
    config["seats"][1]["mcts"] = json::parse(mcts);
    EXPECT_THAT(GameConfigFromJson(config).status().code(),
                Eq(absl::StatusCode::kInvalidArgument))
        << mcts;
  }
}

}  // namespace
}  // namespace blokus
