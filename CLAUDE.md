# CLAUDE.md

A Blokus AI written in C++ and built with Bazel. Read `ARCHITECTURE.md` for
how the code fits together, and keep it up to date when you change
architecture.

## Build, test, run

```bash
bazel build //...
bazel test //...                          # all tests
bazel test //game:board_test --test_output=errors
bazel run -c opt //ai:mcts_benchmark      # perf: always use -c opt
bazel build -c opt //main:blokus && bazel-bin/main/blokus   # run from repo root; serves on 127.0.0.1:7777
bazel run -c opt //main:train -- --num_games=10 --num_players=2 --stderrthreshold=0
bazel run -c opt //main:match -- --num_games=100 --a_c=0.7   # A vs B strength
bazel run -c opt //main:match -- --a_player=random --b_iterations=100
python3 tools/elo.py match_output*.txt    # Elo from main:match "result," lines
# BlokusZero training code (uv project, venv outside the shared folder):
cd zero/train && UV_PROJECT_ENVIRONMENT=~/venvs/blokuszero uv run pytest
UV_PROJECT_ENVIRONMENT=~/venvs/blokuszero uv run python export.py --out ../data/models/random_6x32.onnx
UV_PROJECT_ENVIRONMENT=~/venvs/blokuszero uv run python train.py --data='../data/bootstrap/*.bagz' --out=../data/runs/NAME
bazel run -c opt //zero:evaluator_benchmark -- --model=$PWD/zero/data/models/random_6x32.onnx
bazel run -c opt //zero:generate -- --num_games=100 --output=$PWD/zero/data/bootstrap/test
# After changing zero/training.proto (and before the first pytest run):
bazel run @protobuf//:protoc -- --proto_path=$PWD/zero --python_out=$PWD/zero/train training.proto
```

- Bazel comes from bazelisk, and the version is pinned in `.bazelversion`.
  Dependencies are Bazel modules in `MODULE.bazel` (bzlmod; there is no
  `WORKSPACE`). Commit `MODULE.bazel.lock` together with any dependency
  change.
- Load C++ rules explicitly (`load("@rules_cc//cc:cc_library.bzl", ...)`),
  since Bazel 9 removed the native ones. Proto rules come from
  `@protobuf//bazel:*.bzl`.
- Logging uses Abseil, which only prints WARNING and above to stderr by
  default. Pass `--stderrthreshold=0` to see INFO logs. glog's
  `--alsologtostderr` flag doesn't exist here.
- `bazel test --config=asan //...` runs the tests under ASan and UBSan. Run
  it after any change to `game/` or `ai/` that touches memory, indexing, or
  bit manipulation. `--config=tsan` runs ThreadSanitizer: use it after
  changing threading in `server/` or `ai/mcts.*`.
- `.bazelrc` turns off the `supports_start_end_lib` toolchain feature because
  GNU ld 2.47 segfaults on `--start-lib`. Leave that line in.
- System libraries (installed machine-wide, not through Bazel):
  `libgoogle-perftools-dev`, for `-lprofiler`.
- The project folder is shared from the host, and doesn't count against the
  VM's small disk. Large data (models, self-play data) goes in `zero/data/`
  (git-ignored); venvs go outside the project folder, under `~/venvs/`.
- The dev environment is a VM. If a tool or library is missing, **ask the
  user to install it** instead of installing it yourself.
- Benchmark numbers vary with machine load. Don't run the benchmark while a
  build is going, and compare results only against runs on the same machine.

## Code style

- Follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html):
  - 2-space indent, 80-column lines.
  - `CamelCase` types and functions, `snake_case` variables,
    `trailing_underscore_` members, `kConstantName` constants.
  - Header guards are `BLOKUS_<DIR>_<FILE>_H_`.
  - Include order: the file's own header, then C/C++ system headers, then
    third-party (absl, etc.), then project headers (`"game/board.h"`), with a
    blank line between groups.
  - Everything goes in `namespace blokus`, with an anonymous namespace for
    file-local helpers.
- Use Abseil for logging (`absl/log/log.h`, `absl/log/check.h`), flags,
  strings (`StrCat`, `StrFormat`), and `absl::Span`. Don't use glog or gflags.
- Use `CHECK` for invariants and programmer errors. Game-rule violations
  return `false`; they are not crashes.
- TODOs are written `// TODO(piotrf): ...`.
- Match the comment density of the surrounding code. Public APIs in headers
  get a comment explaining their contract.
- Every new `.cc` file needs a target in its directory's `BUILD` file, with
  explicit deps (one `cc_library` per header/source pair is the convention).
  Use `cc_test` with `@com_google_googletest//:gtest_main` for tests, and use
  gMock matchers (`EXPECT_THAT(x, Eq(...))`) as the existing tests do.
- Name test files `foo_test.cc`, placed next to `foo.cc`.

## Engineering rules

- **Performance matters in `game/board.*` and `ai/mcts.*`.** Rollout speed
  directly limits how strong MCTS can get. Before and after changing
  hot-path code, run `bazel run -c opt //ai:mcts_benchmark` and report the
  numbers. Notable improvements get added to the history table in
  `ai/mcts_benchmark.cc`. Also record results in `benchmark_history.local.md`
  (git-ignored), which tracks numbers on this VM.
- **Back AI-strength claims with `main:match`.** Run enough games for the
  95% CI to exclude 0.5 (100 games gives about ±0.1), and record the
  configs and results in `ARCHITECTURE.md` under "Playing strength".
  Larger investigations get their own write-up in `analysis/` (one `.md` file
  each), with a summary line in "Playing strength".
- `Board` move generation must keep matching the brute-force
  `VerificationBoard` in `game/board_test.cc`. Keep that test passing after
  any change to `Board` or `Tile`.
- `Move` placements must use canonical (rotation, flip) values from
  `Tile::orientations()`. Use `Tile::Canonicalize` on anything coming from
  outside the engine.
- In 2-player games each player controls two colors. Use player ids, not
  colors, for anything about winning or scoring.
- Keep game logic out of the web client (`server/static/`). It gets tile
  shapes and legal moves from the API, and only does geometry (turning
  pieces) and drawing. Rules belong in `game/` and `server/`.
- The client is plain ES modules with no build step or libraries. Check UI
  changes in a browser (light and dark mode, and a narrow window), not just
  by reading the code.

## Git

- Work directly on `master`, one change at a time. Leave changes uncommitted
  so the user can review them; commit only when the user asks.
- `*.org` files (the user's personal notes) and `.vscode/` are git-ignored.
  Don't edit them unless asked.
