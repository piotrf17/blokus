#ifndef BLOKUS_SERVER_JSON_H_
#define BLOKUS_SERVER_JSON_H_

#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"

#include "game/board.h"
#include "game/game.h"

namespace blokus {

// JSON for the web API. Colors are names ("blue"), and cells are
// [row, col] pairs.

// Returns the player id that controls `color` in a `num_players` game.
int PlayerForColor(int num_players, Color color);

// Every tile's orientations, for drawing tiles and previewing moves:
//   [{"tile": 0, "size": 1,
//     "orientations": [{"cells": [[0, 0]]}, ...],
//     "transforms": [[0, 0], [0, 0], [0, 0], [0, 0]]}, ...]
// Each orientation's cells start at row 0 and column 0.
// transforms[rotation][flip] is the index of the orientation that rotating
// the tile `rotation` quarter-turns and then flipping it (if `flip` is 1)
// gives, so a client can rotate and flip a tile without knowing its geometry.
nlohmann::json TilesToJson();

// A move, as {"color": "blue", "tile": 3, "cells": [[0, 0], ...]}, or
// {"color": "blue", "pass": true}.
nlohmann::json MoveToJson(const Move& move);

// The whole game state:
//   {"num_players": 2, "finished": false,
//    "current_color": "blue", "current_player": 0,   (absent once finished)
//    "board": [[null, "blue", ...], ...],            (20 rows of 20 cells)
//    "colors": {"blue": {"player": 0, "score": -89, "passed": false,
//                        "unplayed_tiles": [0, 1, ...]}, ...},
//    "scores": [-178, -178],                         (per player)
//    "winners": [0],                                 (once finished)
//    "moves": [move, ...]}
nlohmann::json GameToJson(const Game& game);

// The current color's legal moves, as a list of moves. Empty if it can only
// pass.
nlohmann::json PossibleMovesToJson(const Game& game);

// Parses a move for the current color from {"tile": 3, "cells": [[r, c],
// ...]} (the cells in any order) or {"pass": true}. Returns an
// InvalidArgument error unless the move is legal. A pass is only legal when
// the current color has no other move.
absl::StatusOr<Move> MoveFromJson(const nlohmann::json& move_json,
                                  const Game& game);

}  // namespace blokus

#endif  // BLOKUS_SERVER_JSON_H_
