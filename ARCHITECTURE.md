# Architecture

An AI for the board game [Blokus](https://en.wikipedia.org/wiki/Blokus),
written in C++ and built with Bazel. The project has four parts: a fast game
engine, AI players (random and Monte Carlo Tree Search), a self-play driver
that can log games to protobuf, and a small web UI so a human can play
against the AI.

```
            main/blokus_main.cc               main/train_main.cc
      (web server + human vs. 3 MCTS)     (MCTS self-play, game logging)
                 |        |                        |         |
          util/http_server |                       |    game/logger ---> game_log.proto
          (libmicrohttpd)  |                       |
                           v                       v
                     game/game_runner  <-- Player interface (game/player.h)
                           |                 ^            ^
                           v                 |            |
                       game/game          ai/mcts      ai/random
                           |
                       game/board
                           |
                  game/tile + game/defs
```

## Directory layout

| Path    | Contents |
|---------|----------|
| `game/` | Engine: coordinates, tiles, board, game rules, game loop, game logging. |
| `ai/`   | `Player` implementations: `RandomAI`, `MctsAI`, plus an MCTS benchmark. |
| `main/` | Binaries (`blokus`, `train`) and the static web client (`main/static/`). |
| `util/` | `HttpServer`, a thin wrapper around libmicrohttpd. |

Everything is in `namespace blokus`.

## Game engine (`game/`)

### `defs.h`: basic types
- `Color`: `INVALID=0, BLUE=1, YELLOW=2, RED=3, GREEN=4`. The values are used
  as array indexes, and turn order is BLUE → YELLOW → RED → GREEN
  (`NextColor`).
- `Coord`: `(row, col)` stored as two `int8_t`. `(0,0)` is the top-left
  corner.
- `Placement`: `coord` + `rotation` (0..3) + `flip`. It says how a tile is
  placed but not which tile.

### `tile.h`: tiles and their precomputed geometry
- `kTiles[21]`: the 21 Blokus pieces, each defined as a 5x5 bool grid and
  indexed 0..20, roughly ordered by size (0 = monomino, 14 = I-pentomino,
  19 = X/cross, ...). It's a global array of non-POD objects built at static
  init time (there's a TODO about this).
- `Tile::Transform(rotation, flip)`: raw transformed coordinates, rotated
  around the 5x5 grid origin. These can be negative.
- `TileOrientation`: one *physically distinct* orientation of a tile. For
  example the monomino has 1 and the L-pentomino has 8. Each one precomputes:
  - `coords()`: normalized so the upper-left of the bounding box is (0,0).
  - `offset()`: where the raw transform origin sits relative to the
    normalized coords. **`Placement::coord` refers to the transform origin,
    not the bounding-box corner**, so the bounding-box upper-left on the board
    is `placement.coord - offset`.
  - `corners()`: blocks of the tile that can touch another same-color piece
    diagonally, each tagged with a direction type.
  - `slots()`: empty cells diagonally adjacent to the tile (and not
    edge-adjacent) where a later same-color piece could attach, each tagged
    with a direction type.
  - `rows()` / `expanded_rows()`: per-row bitmasks of the tile, and of the
    tile grown by one cell in each orthogonal direction. These are used for
    fast overlap checks.
- `CornerFitsSlot`: a lookup table saying which corner types fit which slot
  types.
- `Tile::Canonicalize(placement)`: maps any (rotation, flip) to the matching
  canonical `TileOrientation` and adjusts the coord to match. The web UI
  needs this because it can send non-canonical orientations.

### `board.h`: board state and move generation
- `Move` = `color` + `tile` index (`-1` means pass) + `placement`.
- `Board` is 20x20 and tracks only geometry, not whose turn it is or which
  tiles are left. Its state:
  - `pieces_[r][c]`: the color occupying each cell.
  - `available_[color][row]`: a `uint32_t` bitmask per row. A 1 bit means the
    cell is free *and* not edge-adjacent to that color's own pieces.
  - `slots_[color]`: every slot that color has ever had, deduplicated by
    `slot_map_`. Slots are never removed. Occupancy is checked through
    `available_` instead.
  - `SlotInfo::possible_tiles`: a per-slot bitmask cache of tiles that might
    still fit there. A tile that doesn't fit at a slot can never fit there
    later, so `PossibleMoves` clears bits as it finds failures. The field is
    `mutable` so the const method can update it.
- Move generation (`PossibleMoves(tile, color)`): for each slot × orientation
  × corner, check `CornerFitsSlot`, check the board bounds, and AND the tile's
  row masks against `available_`. A `placement_hashes` array removes duplicate
  placements that different corner/slot pairs can produce.
- Each color starts with one slot at its own corner: BLUE (0,0),
  YELLOW (0,19), RED (19,19), GREEN (19,0).
- `MakeMove` validates the move, writes `pieces_`, adds the new slots, and
  clears bits in `available_`: just the tile cells for other colors, and the
  tile plus its orthogonal neighbors for the moving color.
- `IsPossible(Move)` expects a canonical (rotation, flip). It looks up the
  orientation with `OrientationForMove` and `LOG(FATAL)`s if none matches.

The benchmark history in `ai/mcts_benchmark.cc` records how this design came
about: full-board scan → slots/corners → bitmaps → per-slot tile cache, about
a 120x speedup on random rollouts.

### `game.h`: rules beyond geometry
`Game` holds a `Board` and adds: the current color and player, which tiles
each color still has, which colors have passed, the move history, and
end-of-game scoring.
- 2 or 4 players. In a **2-player game each player controls two colors**:
  player 0 = BLUE+RED, player 1 = YELLOW+GREEN. `current_player_` goes
  0,1,0,1 while colors go B,Y,R,G.
- A pass (`tile == -1`) takes that color out of `players_with_moves_`. Once a
  color has passed it can only pass, and `PossibleMoves()` returns nothing
  for it. The game ends when every color has passed.
- Scoring (`Result()`): minus the number of squares left in hand. Placing all
  21 pieces scores +15, or +20 if the monomino was played last. In 2-player
  games a player's two colors are summed. `winner_id` is the first player
  with the maximum score, so ties go to the lower id.
- `Game` is copied by value freely, for example once per MCTS rollout.

### `player.h`, `game_runner.h`: the game loop
- `Player`: abstract class with `Move SelectMove(const Game&)`. Players get
  ids 0..n-1 and must be added in id order.
- `GameRunner::Play()`: asks the current player for a move, `CHECK`-fails if
  the move is invalid, calls each `ObserverFunc(game, move)`, and repeats
  until the game is over.
- `BoardPrintingObserver`: prints the board to stdout with ANSI colors.

### `logger.h`, `game_log.proto`: game records
`GameLogger::NewGame()` returns an observer that appends each move to a
`GameLog` proto. All games are stored in a `GamesLog` that is serialized to
the output file when the logger is destroyed. There's no metadata or score in
the log yet. The proto `Color` enum mirrors `defs.h`.

## AI (`ai/`)

### `RandomAI`
Picks uniformly from `game.PossibleMoves()`, or passes if there are none.

### `MctsAI`: Monte Carlo Tree Search with UCB1
- `Node`: incoming `move`, the `player` who made it, `wins`, `visits`,
  `parent`, and `children`.
- **Selection + expansion** (`SelectNode`): descend by UCB1
  (`wins/visits + c*sqrt(ln N / visits)`). Unvisited children score +∞, and
  ties are broken at random. A leaf is expanded only after its parent has at
  least as many visits as it has children. If there are no legal moves, the
  node gets a single pass child.
- **Rollout** (`Rollout`): uniformly random moves for every color until the
  game ends. Returns the winning player id; the score margin isn't used.
- **Backprop**: walk up to the root and increment `visits`, plus `wins` on
  nodes whose `player` won. Because `player` is a player id rather than a
  color, this works for 2-player games where one player has two colors.
- **Parallelism**: `num_threads` workers share a single tree.
  Selection/expansion and backprop happen under one `tree_mutex_`, and
  rollouts run outside the lock. All randomness comes from global
  `rand()`/`srand()`.
- **Tree reuse**: on each `SelectMove`, the AI walks the stored tree down
  through the moves made since its last turn (`game.moves()`) and keeps that
  subtree. If a move isn't in the tree (it was never expanded that far, or a
  color passed while it still had moves), it starts a new tree.
- **Move choice**: the most-visited root child. If there's only one legal
  move it's played immediately.
- `MctsOptions`: `c` (1.4), `num_iterations`, `num_rollouts_per_iteration`,
  `num_threads`.

## Binaries (`main/`)

### `main:blokus`: play against the AI in a browser
Starts `HttpServer` on **port 7777**, serving the static files in
`main/static/` by **relative path**, so run it from the repo root. Seats a
`WebPlayer` (human, BLUE) against three `MctsAI`s (YELLOW and GREEN weak at
20k iterations, RED strong at 200k iterations; 8 threads each).
- `POST /game/<player_id>/place` (JSON `{tile, move:{coord,rotation,flip}}`,
  or `{tile:-1}` to pass): the human's move. `WebPlayer::SelectMove` polls
  until this arrives, then canonicalizes the placement.
- `GET /game/next_move`: long-poll. `MoveForwarder` queues every move
  (including the human's, which the client skips when `player == 0`) and
  hands them out one at a time.

### `main:train`: self-play
MCTS-vs-MCTS games with flags `--num_games`, `--num_players`,
`--num_mcts_iterations`, `--num_mcts_rollouts`, `--num_mcts_threads`,
`--seed`, `--print_board`, and `--output_path` (writes a `GamesLog` proto).
Prints average scores at the end. This is meant to grow into the data
pipeline for a "BlokusZero"-style learned policy or value function.

### `ai:mcts_benchmark`
Google Benchmark for `Rollout` and `SelectMove`. Links `-lprofiler`
(gperftools) for CPU profiling; the file header has the `pprof` commands.

## Web client (`main/static/`)

Plain ES modules plus a vendored jQuery 3.1.0, drawn on a `<canvas>`.
`board.js` and `tiles.js` **re-implement the board and tile logic in JS**
using the older brute-force "frontier/allowed" algorithm, which is also
`VerificationBoard` in `board_test.cc`. **Colors are bit flags in JS
(1,2,4,8) but 1..4 in C++.** The seat setup is hard-coded (`PLAYERS`).
Controls: click a tile, `r` to rotate, `f` to flip, `p` to pass.

## Utilities (`util/`)

`HttpServer`: exact-match URL routing to either a static file or a
`std::function` handler. JSON POST bodies are parsed with jsoncpp. It keeps a
single `post_processor_` for the whole server, so concurrent POSTs aren't
safe. That's fine for a one-human local game.

## Tests

- `game:defs_test`: `Color`, `Coord`, and `Placement` helpers.
- `game:tile_test`: a few hand-written examples, plus exhaustive checks over
  every tile and orientation against independent brute-force derivations:
  piece sizes and orientation counts (89 squares, 91 orientations),
  normalization and `offset`, row bitmaps, corner and slot sets and their
  types, `Canonicalize` keeping the same cells, and the `CornerFitsSlot`
  table derived from geometry.
- `game:board_test`: hand-checked cases covering the start corners,
  off-board placements, same-color and other-color adjacency, and invalid
  moves leaving the board unchanged. Plus comparisons against the brute-force
  `VerificationBoard`, which reuses the old frontier/allowed algorithm:
  - `PossibleMovesMatchVerificationBoard` plays 20 random games to the end
    and compares `PossibleMoves` for every remaining tile before each move.
  - `IsPossibleMatchesVerificationBoard` plays 2 games and, before each move,
    compares `IsPossible` for every orientation at every position.
- `game:game_test`: hand-checked cases for turn order (4 and 2 players),
  wrong-color and illegal moves, used tiles, passing being permanent, the end
  of the game, and scoring, including ties and 2-player sums. The +15/+20
  bonus tests find a full 21-tile placement for BLUE with a depth-first
  search while the other colors pass.
- `game:game_runner_test`: scripted players check that each player is asked
  only for its own colors (4 and 2 players), that observers see every move
  after it's made, that the runner owns and destroys its players, and the
  `CHECK`s on player setup and invalid moves.
- `game:logger_test`: logs two games, then parses the file back and compares
  every move.
- `bazel test --config=asan //...` runs the tests under AddressSanitizer and
  UndefinedBehaviorSanitizer. It is slow (`board_test` takes about 100 s).
- `ai:mcts_test`: rollouts return a valid player id, the opening move is
  legal, and full games with tree reuse for 2 and 4 players and with 4
  threads finish. Also checks that the AI copes when an opponent passes while
  it still has moves. The AIs use only 10 to 200 iterations, so these check
  correctness, not playing strength.
- There are no tests for `RandomAI`, `HttpServer`, or the JS client.

## Build and dependencies

- Bazel 9 (pinned in `.bazelversion`, run through bazelisk) with bzlmod.
  `MODULE.bazel` lists abseil-cpp, googletest, google_benchmark, protobuf,
  and rules_cc from the Bazel Central Registry. Resolved versions are locked
  in `MODULE.bazel.lock`.
- C++20 (`.bazelrc`, for both target and host builds).
- `.bazelrc` also turns off `supports_start_end_lib`, because GNU ld 2.47
  segfaults on the `--start-lib/--end-lib` flags Bazel would otherwise pass.
- System libraries linked with `linkopts`: libmicrohttpd and jsoncpp (for
  `main:blokus`), and gperftools `-lprofiler` (for `ai:mcts_benchmark` and
  `main:train`).
- Logging, flags, and CHECK come from Abseil (glog was removed). `VLOG` calls
  are commented out with "re-enable once absl supports vlog"; newer Abseil
  has `VLOG`.

## Known issues and sharp edges

From reading the code. These haven't been reproduced by a test.

- The build has many `-Wsign-compare` warnings (`int` loop indexes compared
  against `.size()`).
- Global `rand()` is shared by all MCTS threads, which is a likely source of
  contention and makes multi-threaded runs non-reproducible.
- `Game::MakeMove` accepts a pass even when the color still has legal moves.
  The AIs only pass when they have no moves, but a human can pass early.
- `Rollout` returns `GameResult::winner_id`, which gives ties to the lowest
  player id. That biases MCTS toward player 0 in tied rollouts.
- Nothing range-checks `Move::tile`. `blokus_main.cc` indexes `kTiles` with
  the tile from the HTTP request, and `Game::MakeMove` indexes its tile
  table, so an out-of-range tile is undefined behavior.
- `HttpServer::Request::status_` is uninitialized. `blokus.js` compares
  `PLAYERS[i] == 'computer'` (an object against a string) in its constructor.
- `notes.org` holds personal notes and is git-ignored (`*.org`).
