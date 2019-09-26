#include "ai/mcts.h"

#include <glog/logging.h>

namespace blokus {

using TileMove = std::pair</*tile*/ int, Move>;

bool MctsAI::SelectMove(const Board& board, Move* move, int* chosen_tile) {
  std::vector<std::pair<int, Move>> moves;
  for (int tile = 20; tile >= 0; tile --) {  // xxx makes less sense here than randomai
    if (played_tiles_.count(tile) > 0) continue;
    auto tile_moves = board.PossibleMoves(kTiles[tile], color());
    for (const Move& move : tile_moves) {
      moves.push_back({tile, move});
    }
  };
  LOG(INFO) << "Found " << moves.size() << " moves.";
  if (moves.size() == 0) {
    return false;
  } else if (moves.size() == 1) {
    *move = moves[0].second;
    *chosen_tile = moves[0].first;
    played_tiles_.insert(*chosen_tile);
  } else {
    int choice = rand() % moves.size();
    *move = moves[choice].second;
    *chosen_tile = moves[choice].first;
    played_tiles_.insert(*chosen_tile);
    return true;
  }
  return false;
}

}  // namespace blokus
