#include "server/json.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"

#include "game/defs.h"
#include "game/tile.h"

namespace blokus {
namespace {

using ::nlohmann::json;

constexpr Color kColors[] = {BLUE, YELLOW, RED, GREEN};

json CellsToJson(const std::vector<Coord>& cells) {
  json result = json::array();
  for (const Coord& cell : cells) {
    result.push_back({cell.row(), cell.col()});
  }
  return result;
}

// Sorts cells by (row, col), so that two moves' cells can be compared.
std::vector<std::pair<int, int>> SortedCells(const std::vector<Coord>& cells) {
  std::vector<std::pair<int, int>> sorted;
  for (const Coord& cell : cells) sorted.emplace_back(cell.row(), cell.col());
  std::sort(sorted.begin(), sorted.end());
  return sorted;
}

// Returns the index of `tile`'s orientation with `rotation` and `flip`.
int OrientationIndex(const Tile& tile, int rotation, bool flip) {
  const Placement canonical = tile.Canonicalize({Coord(0, 0), rotation, flip});
  const std::vector<TileOrientation>& orientations = tile.orientations();
  for (int i = 0; i < static_cast<int>(orientations.size()); ++i) {
    if (orientations[i].rotation() == canonical.rotation &&
        orientations[i].flip() == canonical.flip) {
      return i;
    }
  }
  LOG(FATAL) << "No orientation of tile " << tile.index() << " for rotation "
             << rotation << ", flip " << flip;
}

}  // namespace

int PlayerForColor(int num_players, Color color) {
  CHECK(color != INVALID);
  return (color - 1) % num_players;
}

json TilesToJson() {
  json tiles = json::array();
  for (int index = 0; index < kNumTiles; ++index) {
    const Tile& tile = kTiles[index];
    json orientations = json::array();
    for (const TileOrientation& orientation : tile.orientations()) {
      const absl::Span<const Coord> coords = orientation.coords();
      orientations.push_back(
          {{"cells", CellsToJson({coords.begin(), coords.end()})}});
    }
    json transforms = json::array();
    for (int rotation = 0; rotation < 4; ++rotation) {
      transforms.push_back({OrientationIndex(tile, rotation, false),
                            OrientationIndex(tile, rotation, true)});
    }
    tiles.push_back({{"tile", index},
                     {"size", tile.Size()},
                     {"orientations", std::move(orientations)},
                     {"transforms", std::move(transforms)}});
  }
  return tiles;
}

json MoveToJson(const Move& move) {
  if (move.tile == -1) {
    return {{"color", ColorToString(move.color)}, {"pass", true}};
  }
  return {{"color", ColorToString(move.color)},
          {"tile", move.tile},
          {"cells", CellsToJson(move.Cells())}};
}

json GameToJson(const Game& game) {
  json state;
  state["num_players"] = game.num_players();
  state["finished"] = game.Finished();
  if (!game.Finished()) {
    state["current_color"] = ColorToString(game.current_color());
    state["current_player"] = game.current_player();
  }

  json board = json::array();
  for (int row = 0; row < Board::kNumRows; ++row) {
    json cells = json::array();
    for (int col = 0; col < Board::kNumCols; ++col) {
      const Color color = game.board().At(row, col);
      cells.push_back(color == INVALID ? json(nullptr)
                                       : json(ColorToString(color)));
    }
    board.push_back(std::move(cells));
  }
  state["board"] = std::move(board);

  json colors = json::object();
  std::vector<int> scores(game.num_players(), 0);
  for (Color color : kColors) {
    json unplayed = json::array();
    for (int tile = 0; tile < kNumTiles; ++tile) {
      if (game.IsTileUnplayed(color, tile)) unplayed.push_back(tile);
    }
    const int player = PlayerForColor(game.num_players(), color);
    const int score = game.ColorScore(color);
    scores[player] += score;
    colors[ColorToString(color)] = {{"player", player},
                                    {"score", score},
                                    {"passed", game.HasPassed(color)},
                                    {"unplayed_tiles", std::move(unplayed)}};
  }
  state["colors"] = std::move(colors);
  state["scores"] = scores;

  if (game.Finished()) {
    const int best = *std::max_element(scores.begin(), scores.end());
    json winners = json::array();
    for (int player = 0; player < game.num_players(); ++player) {
      if (scores[player] == best) winners.push_back(player);
    }
    state["winners"] = std::move(winners);
  }

  json moves = json::array();
  for (const Move& move : game.moves()) moves.push_back(MoveToJson(move));
  state["moves"] = std::move(moves);
  return state;
}

json PossibleMovesToJson(const Game& game) {
  json moves = json::array();
  if (game.Finished()) return moves;
  for (const Move& move : game.PossibleMoves()) {
    moves.push_back(MoveToJson(move));
  }
  return moves;
}

absl::StatusOr<Move> MoveFromJson(const json& move_json, const Game& game) {
  if (game.Finished()) {
    return absl::InvalidArgumentError("The game is finished");
  }
  if (!move_json.is_object()) {
    return absl::InvalidArgumentError("A move must be a JSON object");
  }
  const Color color = game.current_color();

  if (move_json.contains("pass")) {
    if (move_json["pass"] != true) {
      return absl::InvalidArgumentError("\"pass\" must be true");
    }
    if (game.NumPossibleMoves() > 0) {
      return absl::InvalidArgumentError(
          "Can't pass while there are moves to play");
    }
    return Move::EmptyMove(color);
  }

  if (!move_json.contains("tile") || !move_json["tile"].is_number_integer()) {
    return absl::InvalidArgumentError("A move needs an integer \"tile\"");
  }
  const int tile = move_json["tile"].get<int>();
  if (tile < 0 || tile >= kNumTiles) {
    return absl::InvalidArgumentError("\"tile\" is out of range");
  }
  if (!move_json.contains("cells") || !move_json["cells"].is_array()) {
    return absl::InvalidArgumentError("A move needs a \"cells\" array");
  }
  std::vector<std::pair<int, int>> cells;
  for (const auto& cell : move_json["cells"]) {
    if (!cell.is_array() || cell.size() != 2 ||
        !cell[0].is_number_integer() || !cell[1].is_number_integer()) {
      return absl::InvalidArgumentError(
          "Each cell must be a [row, col] pair of integers");
    }
    cells.emplace_back(cell[0].get<int>(), cell[1].get<int>());
  }
  std::sort(cells.begin(), cells.end());

  for (const Move& move : game.PossibleMoves()) {
    if (move.tile == tile && SortedCells(move.Cells()) == cells) return move;
  }
  return absl::InvalidArgumentError("That move isn't legal");
}

}  // namespace blokus
