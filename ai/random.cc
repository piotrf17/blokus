#include "ai/random.h"

namespace blokus {

bool RandomAI::SelectMove(const Board& board, Move* move, int* chosen_tile) {
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

}  // namespace blokus
