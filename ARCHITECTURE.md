# Architecture

An AI for the board game [Blokus](https://en.wikipedia.org/wiki/Blokus),
written in C++ and built with Bazel. The project has four parts: a fast game
engine, AI players (random and Monte Carlo Tree Search), a self-play driver
that can log games to protobuf, and a web server for playing against the AI
in a browser.

```
 main/blokus_main.cc                      main/train_main.cc, main/match_main.cc
        |                                              |
 server/web_server (HTTP, cpp-httplib)                 |    game/logger --> game_log.proto
        |                                              v
 server/game_manager (all games, AI thread)     game/game_runner
        |                                              |
 server/game_session (one game)                        |
   uses server/config (seats, presets),                |
        server/json (API JSON),                        |
        Players for AI seats                           |
        |                                              |
        |        Player interface (game/player.h): ai/mcts, ai/random
        |                                              |
        `--------------------> game/game <-------------'
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
| `main/` | Binaries (`blokus`, `train`, `match`). |
| `server/` | The web server (JSON API, game configs, sessions, the game manager, HTTP) and the web client (`server/static/`). |
| `tools/` | Scripts, e.g. `elo.py`, which fits Elo ratings to `main:match` results. |
| `analysis/` | Write-ups of strength investigations, with their raw data in `analysis/data/`. |
| `zero/` | BlokusZero, in progress (see `zero/PLAN.md`): `zero/train/` is the Python training code, and `zero/data/` (git-ignored) holds models and training data. |

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
- `SlotWindow` / `Tile::CornerPlacements(slot_type)`: every tile placed with
  a corner on a slot stays inside an 8x8 window around the slot, whose
  position depends only on the slot type. For each slot type, a tile
  precomputes every (orientation, fitting corner) as a placement relative to
  the slot plus a 64-bit mask of the squares it covers in that window.
  `TileOrientation::packed_rows()` holds the row bitmaps 5 bits per row.
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
  - `slots_[color]`: every slot that color has ever had, deduplicated by the
    `slot_rows_` bitmap. Slots are never removed. Occupancy is checked
    through `available_` instead.
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
- **Incremental move tracking** (`AllPossibleMoves(color)`,
  `NumPossibleMoves`, `PossibleMove(color, i)`): `tracked_moves_[color]` holds
  every possible move for the tiles that color hasn't placed, as compact
  `TrackedMove`s. It relies on two facts: `available_` only loses cells, and
  slots are only added. So after each `MakeMove`:
  - every color drops the tracked moves that now overlap unavailable cells
    (only moves whose bounding box meets the changed area are rechecked), and
    the moving color drops all moves with the placed tile;
  - the moving color adds the moves through its new slots: each of a tile's
    `CornerPlacements` for the slot type is possible if its mask is inside
    the slot's window of `available_` (one AND; squares off the board are
    never in the window). A move that also covers an older slot was already
    possible, so it's skipped, which keeps the list free of duplicates
    without a hash set.
  The result is identical, in the same order, to calling `PossibleMoves` for
  each remaining tile, which `board_test` checks over full games.
  `Game::PossibleMoves` and `Rollout` use it. `PossibleMoves(tile, color)`
  still computes from scratch.
- `IsPossible(Move)` expects a canonical (rotation, flip). It looks up the
  orientation with `OrientationForMove` and `LOG(FATAL)`s if none matches.

The benchmark history in `ai/mcts_benchmark.cc` records how this design came
about: full-board scan → slots/corners → bitmaps → per-slot tile cache, about
a 120x speedup on random rollouts. Incremental move tracking then gave
about another 5x.

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
  with the maximum score, so ties go to the lower id. MCTS doesn't use it;
  it splits ties with `Rewards()` instead.
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
Takes an optional seed for its `std::mt19937_64`.

### `MctsAI`: Monte Carlo Tree Search with UCB1
- `Node`: incoming `move`, the `player` who made it, `wins`, `visits`,
  `parent`, and `children`.
- **Selection + expansion** (`SelectNode`): descend by UCB1
  (`wins/visits + c*sqrt(ln N / visits)`). Unvisited children score +∞, and
  ties are broken at random. A leaf is expanded only after its parent has at
  least as many visits as it has children. If there are no legal moves, the
  node gets a single pass child.
- **Rollout** (`Rollout`): random moves (`RolloutMove`) for every color
  until the game ends. Each move is weighted by `exp(beta * tile size)`,
  with `beta` = `RolloutOptions::size_bias` (default 1): it counts moves per
  tile with `Game::NumPossibleMovesByTile`, picks a tile by count times
  weight, then a uniform move of that tile with
  `Game::PossibleMoveWithTile`. At `size_bias` 0 moves are uniform: it picks
  an index with `Game::NumPossibleMoves` and fetches only that move with
  `Game::PossibleMove`, instead of building the full list. Both cost about
  the same.
  Returns `Rewards(result)`: 1 for the sole top scorer, or 1/k for each of k
  players tied for the top score (ties happen in about 3% of random 2-player
  rollouts and 8% of 4-player ones). With `margin_weight` w > 0 (default 0)
  that is mixed with a margin reward: `(1 - w) * win + w * (0.5 + 0.5 *
  tanh(margin / margin_scale))`, where the margin is over the best other
  player.
- **Backprop**: walk up to the root, incrementing `visits` and adding the
  reward of each node's `player` to its `wins` (a `double`). Because `player` is a player id rather than a
  color, this works for 2-player games where one player has two colors.
- **Parallelism**: `num_threads` workers share a single tree.
  Selection/expansion and backprop happen under one `tree_mutex_`, and
  rollouts run outside the lock. Each worker thread has its own
  `std::mt19937_64`, seeded from the AI's generator at the start of every
  `SelectMove`. `MctsOptions::seed` seeds the AI (from `std::random_device`
  if unset). With a seed and one thread, play is reproducible. With more
  threads it isn't, because the threads interleave on the shared tree.
  Throughput scales well up to the core count (10k iterations: 1 thread
  2.1 s, 4 threads 0.62 s on the 4-vCPU VM), and more threads than cores is
  slower. `MctsOptions::virtual_loss` (default 3, only used with more than
  one thread) adds temporary losses along the selected path so threads
  spread out. At `c = 0.25` it measured as neutral to slightly helpful
  (see "Playing strength" below).
- **Tree reuse**: on each `SelectMove`, the AI walks the stored tree down
  through the moves made since its last turn (`game.moves()`) and keeps that
  subtree. If a move isn't in the tree (it was never expanded that far, or a
  color passed while it still had moves), it starts a new tree.
- **Move choice**: the most-visited root child, breaking ties by total
  reward and then randomly. If there's only one legal move it's played
  immediately.
- `MctsOptions`: `c` (0.25), `num_iterations`, `num_rollouts_per_iteration`,
  `rollout` (`RolloutOptions`: `size_bias` 1, `margin_weight` 0,
  `margin_scale` 10), `num_threads`, `virtual_loss`,
  `seed`.

### Playing strength
Elo by iteration count, with random play at 0 (2-player, `c = 0.25`; see
[`analysis/mcts_iterations.md`](analysis/mcts_iterations.md)): 10 iterations
301, 100: 630, 1000: 1312, 10k: 2163, 100k: 2521. Each 3x in iterations is
worth about 450 Elo between 300 and 3000, and under 200 above 10k.

Tuning (see [`analysis/mcts_tuning.md`](analysis/mcts_tuning.md)):
- `c = 1.4` explores far too much. The default `c = 0.25` is the best value
  found at 10k iterations, and is as good as anything lower at 1000.
- At `c = 0.25`, 4 threads play as well as 1 thread at 2.8x the speed.
  Virtual loss is neutral to slightly helpful, and defaults to 3.
- Move-choice ties go to the higher total reward, then a random child.
  Taking the first child favored small tiles and made low-budget MCTS much
  weaker.

Rollouts (see [`analysis/mcts_rollouts.md`](analysis/mcts_rollouts.md)):
- Favoring larger tiles (`size_bias` 1) scores 0.65 against uniform
  rollouts at 1000 iterations, but only 0.52 ± 0.048 at 10k (424 games, not
  significant).
- Adding score-margin rewards (`margin_weight` 0.5) to that beats the
  default 0.585 ± 0.046 at 10k (440 games), and wins by 16 points on
  average. Against `size_bias` 1 alone, margin rewards score 0.47 ± 0.065
  at 10k: they make wins bigger, not more frequent. `size_bias` 1 is the
  default; margin rewards are off by default.
- Best-of-k rollout moves by corner heuristics made MCTS weaker, and were
  removed.

## Binaries (`main/`)

### `main:blokus`: the web server
Runs a `GameManager` and a `WebServer` (see "Web server" below). Flags:
`--host` (default `127.0.0.1`), `--port` (7777), `--data_dir` (`games`,
git-ignored) and `--static_dir` (`server/static`). The paths are relative,
so run it from the repo root. It logs INFO by default. SIGINT and SIGTERM
are handled on their own thread: it closes the games, which ends any
long-polls, then stops the server, and the `GameManager` waits for a
running AI search before exiting.

### `main:train`: self-play
MCTS-vs-MCTS games with flags `--num_games`, `--num_players`,
`--num_mcts_iterations`, `--num_mcts_rollouts`, `--num_mcts_threads`,
`--seed`, `--print_board`, and `--output_path` (writes a `GamesLog` proto).
`--seed` derives a seed for each AI. Prints average scores at the end. This
is meant to grow into the data pipeline for a "BlokusZero"-style learned
policy or value function.

### `main:match`: measure playing strength
Plays 2-player games between two configurations, A and B, swapping seats
every game. Each is an MCTS AI (flags `--a_iterations`, `--a_threads`,
`--a_c`, `--a_virtual_loss`, and the same for B) or the random AI
(`--a_player=random`). Prints a line after each game with A's running
wins, ties, and losses. At the end it prints A's totals, its mean score
with a 95% confidence interval, the mean score margin, each side's total
time in `SelectMove`, and a `result,<A>,<B>,<wins>,<ties>,<losses>` line
for `tools/elo.py`.

### `ai:mcts_benchmark`
Google Benchmark for `Rollout` (uniform, and with `size_bias` 1 and 4) and
`SelectMove` (10,000 iterations on 1, 2,
4, and 8 threads, timed in wall time; items/sec is MCTS iterations). Links `-lprofiler`
(gperftools) for CPU profiling; the file header has the `pprof` commands.

## Web server (`server/`)

The server owns the game state and rules, and the browser only draws, so
the JS has no game logic.

- `server/json`: the API's JSON (nlohmann_json). `GameToJson` (board, each
  color's score, unplayed tiles and pass state, player scores, winners,
  moves), `PossibleMovesToJson`, and `TilesToJson` (each tile's orientations
  as cells, and which orientation each rotation and flip gives). Moves are
  sent as a tile and its board cells. `MoveFromJson` matches those against
  the legal moves, so a client never deals with the engine's rotation and
  flip conventions. It also rejects a pass while moves remain.
- `server/config`: `GameConfig`, a `SeatConfig` (human, or AI with resolved
  `MctsOptions`) per player id, parsed from and written to JSON. AI presets
  from the Elo ladder: `low` (300 iterations), `medium` (3k) and `high`
  (100k, one thread per hardware thread). A `custom` seat sets any MCTS
  option, range-checked.
- `server/game_session`: one game. Humans move with `PlayMove`. AI turns run
  through `RunAiTurn`, which searches on a copy of the game without holding
  the lock, and applies the move only if nothing changed meanwhile. A color
  with no moves passes automatically. Every change bumps a `version`:
  changes carry the version they're based on and are rejected if it's stale,
  and `WaitForState` long-polls for the next one. `Position(n)` replays the
  first `n` moves, for reviewing a game. `Undo` goes back to before
  the last tile a human placed, and remakes the AIs so they don't reuse
  stale trees. The session saves itself as JSON (config and moves) after
  every change, via a temporary file and a rename.
- `server/game_manager`: all sessions, keyed by a random 12-hex-digit id. It
  loads saved games from a data directory at startup, and runs a single AI
  thread that serves the games needing an AI turn in round-robin order,
  since one search already uses every core.
- `server/web_server`: the HTTP API on cpp-httplib, plus the static client
  files. The endpoints are listed in `web_server.h`. Errors are
  `{"error": ...}` with 400 (bad request), 404 (unknown game) or 409 (stale
  version, or a move out of turn). Clients long-poll with
  `GET /api/games/<id>?wait_version=N`, which returns after up to 25 s.
  Long-polls hold a thread each, so the server uses a pool of 32.

### Web client (`server/static/`)

Plain ES modules and one stylesheet, with no build step or libraries. It has
no game logic: tile shapes come from `/api/tiles`, legal placements from
`/legal_moves`, and past positions from `/position`.
- `index.html`/`index.js`: start a 2- or 4-player game, choosing Human, a
  preset AI, or a custom AI (any MCTS option) for each player, and list,
  open or delete existing games.
- `game.html`/`game.js`: the board (SVG), sized to the window's height,
  and a side column with the tray of pieces for the color to move (on a
  human's turn) and each player's score and pieces left. Below 900 px wide
  they stack. Pick a piece, turn it (`R`/`Shift+R`/scroll, `F`), and click
  to place it. A preview shows whether the hovered placement is legal. Dots
  marking where the piece fits are off by default; `H` or a checkbox
  toggles them, remembered in `localStorage`. Turning a piece is pure geometry:
  rotate or mirror its cells, then look up which of the tile's orientations
  has that shape. A placement is legal if its orientation and position are
  in the legal-move list. On a touch screen, the first tap previews and a
  second tap places.
- A finished game can be reviewed piece by piece (buttons, or the arrow,
  `Home` and `End` keys), skipping passes. Each position is fetched from
  `/position` and cached, and steps taken while one loads add up.
- The page long-polls for new states and accepts only newer versions,
  except that an older version from a long-poll means the server
  restarted. It reconnects with backoff, so refreshing, restarting the
  server, or a second tab all stay in sync.
- `api.js` wraps the API, and `common.js` has DOM and labeling helpers.
  `style.css` uses CSS variables, with a dark theme for
  `prefers-color-scheme: dark`.

## Tests

- `game:defs_test`: `Color`, `Coord`, and `Placement` helpers.
- `game:tile_test`: a few hand-written examples, plus exhaustive checks over
  every tile and orientation against independent brute-force derivations:
  piece sizes and orientation counts (89 squares, 91 orientations),
  normalization and `offset`, row bitmaps, corner and slot sets and their
  types, `Canonicalize` keeping the same cells, and the `CornerFitsSlot`
  table derived from geometry.
- `game:board_test`: hand-checked cases covering the start corners,
  off-board placements, same-color and other-color adjacency, invalid
  moves leaving the board unchanged, and `Move::Cells` and `Board::At` (for
  every opening move). Plus comparisons against the brute-force
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
- `ai:mcts_test`: rollouts return a valid player id, biased rollouts finish
  and are reproducible, `RolloutMove` with no bias matches a uniform pick and
  with a bias matches the expected tile-size distribution, margin rewards
  match hand-computed values, the opening move is
  legal, and full games with tree reuse for 2 and 4 players and with 4
  threads finish. Also checks that the AI copes when an opponent passes while
  it still has moves, and that virtual loss leaves single-threaded play
  unchanged and works with 4 threads. The AIs use only 10 to 200
  iterations, so these check correctness, not playing strength. A seeded single-threaded game is
  checked to be reproducible.
- `ai:random_test`: full games, passing with no moves, and reproducibility.
- `server:json_test`: the JSON for tiles, moves and game states (initial,
  mid-game, finished, ties), and `MoveFromJson` round-tripping the legal
  moves of 8 turns with shuffled cells, rejecting malformed and illegal
  moves, and allowing a pass only without moves.
- `server:config_test`: presets, human, preset and custom seats, round
  trips, and rejecting bad configs and MCTS options.
- `server:game_session_test` and `server:game_manager_test` use a fake AI
  (`server/test_ai.h`) that plays the first legal move, and can be held
  mid-search by a `Gate`. They cover moves, stale versions, AI turns, forced
  passes, undo (including during a search), positions, long-polling,
  closing, saving and loading, AI-only games finishing, listing, deleting,
  restarting the manager mid-game, and skipping bad save files.
  `--config=tsan` runs them under ThreadSanitizer.
- `server:web_server_test`: runs the server on a free port and calls every
  endpoint over HTTP with httplib's client: errors and status codes, an AI
  reply arriving through a long-poll, undo, positions, long-poll timeouts,
  closing games ending long-polls, deleting, and static files.
- There are no automated tests for the JS client.

## Build and dependencies

- Bazel 9 (pinned in `.bazelversion`, run through bazelisk) with bzlmod.
  `MODULE.bazel` lists abseil-cpp, cpp-httplib, googletest,
  google_benchmark, nlohmann_json, protobuf, and rules_cc from the Bazel
  Central Registry. cpp-httplib pulls in boringssl and zlib, which makes
  the first build slower; our code doesn't use TLS (Caddy can do that). Resolved versions are locked
  in `MODULE.bazel.lock`.
- C++20 (`.bazelrc`, for both target and host builds).
- `.bazelrc` also turns off `supports_start_end_lib`, because GNU ld 2.47
  segfaults on the `--start-lib/--end-lib` flags Bazel would otherwise pass.
- The only system library is gperftools `-lprofiler`, linked with
  `linkopts` (for `ai:mcts_benchmark` and `main:train`).
- Logging, flags, and CHECK come from Abseil (glog was removed). `VLOG` calls
  are commented out with "re-enable once absl supports vlog"; newer Abseil
  has `VLOG`.

## BlokusZero (`zero/`)

In progress; [`zero/PLAN.md`](zero/PLAN.md) has the design and milestones.

- `zero/train/` is a uv project (PyTorch on CPU, ONNX, ONNX Runtime), outside
  Bazel. Its venv is `~/venvs/blokuszero`, on the VM disk rather than the
  shared project folder, so uv commands need
  `UV_PROJECT_ENVIRONMENT=~/venvs/blokuszero`.
- `model.py` is the network: a ResNet (`NetConfig`: blocks, channels) taking
  12 board planes and 88 global features, with a 91 x 400 policy head
  (orientation, top-left cell), win/draw/loss logits and a score margin. The
  input and policy layouts must match the C++ encoder (milestone 2).
- `export.py` writes ONNX with a dynamic batch size (random weights without
  `--checkpoint`). `model_test.py` checks shapes, the policy layout, and that
  ONNX Runtime matches PyTorch.

## Known issues and sharp edges

From reading the code. These haven't been reproduced by a test.

- The build has many `-Wsign-compare` warnings (`int` loop indexes compared
  against `.size()`).
- `c` was tuned at 1000-20k iterations. The strong web AI uses 200k, where
  the best `c` may be higher, but that's too slow to tune with `main:match`
  on the VM.
- `Game::MakeMove` accepts a pass even when the color still has legal moves.
  Tests use this to set up positions. `server/json` rejects early passes
  from the web.
- `notes.org` holds personal notes and is git-ignored (`*.org`).
