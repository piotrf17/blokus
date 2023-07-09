#include "game/logger.h"

#include "absl/log/check.h"

namespace blokus {
namespace {

GameLog::Move ToProto(const Move& move) {
  GameLog::Move proto;
  proto.set_color(static_cast<GameLog::Color>(move.color));
  proto.set_tile(move.tile);
  proto.mutable_placement()->set_row(move.placement.coord.row());
  proto.mutable_placement()->set_col(move.placement.coord.col());
  proto.mutable_placement()->set_rotation(move.placement.rotation);
  proto.mutable_placement()->set_flip(move.placement.flip);
  return proto;
}

}  // namespace

GameLogger::GameLogger(std::string_view output_path) {
  // Open the file in the constructor so that we can fail fast if there is an
  // issue with opening it.
  outfile_.open(output_path.data(), std::ios::trunc | std::ios::binary);
  CHECK(outfile_.is_open());
}

GameLogger::~GameLogger() {
  CHECK(all_games_.SerializeToOstream(&outfile_));
  outfile_.close();
}

ObserverFunc GameLogger::NewGame() {
  current_ = all_games_.add_games();
  return [this](const Game& game, const Move& move) {
    AddMove(game, move);
  };
}

void GameLogger::AddMove(const Game& game, const Move& move) {
  *current_->add_moves() = ToProto(move);
}

}  // namespace blokus
