# BlokusZero plan

An AlphaZero-style Blokus AI: a small policy/value network guides the tree
search instead of random rollouts, and both improve by self-play. Everything
runs on the dev VM, which has no GPU.

## Constraints

The VM sets the scale of everything below:

- **CPU:** 8 vCPUs, reported as an i7-10510U (a 4-core laptop part), with
  AVX2 and FMA but no AVX-512 or VNNI. Peak FP32 is roughly 300 GFLOPS. Small
  convolutions on a 20x20 board will probably sustain 30–100 GFLOPS across
  all cores. **This is the number to measure first** (milestone 2).
- **Memory:** 7 GB of RAM. Fine for a replay buffer of a few million
  positions.
- **Disk:** about 8 GB free on the VM disk, enough for the venv (CPU-only
  PyTorch takes about 1 GB). The project folder is a share from the host
  with 45 GB free, and doesn't count against the VM disk. Self-play shards,
  models and game records go there, in the git-ignored `zero/data/`. The
  venv must not go there.
- **Today's MCTS** is the baseline to beat: 10k iterations take about 2.5 s
  per move on one thread, at about 2160 Elo on the scale in
  [`analysis/mcts_iterations.md`](../analysis/mcts_iterations.md). 100k
  iterations reach about 2520.

## Scope

2-player standard Blokus first: a 20x20 board, with each player controlling
two colors. The engine already supports it, and two-sided values are simpler
than four-sided ones. 4-player games reuse the same network with a 4-way
value head later.

The fallback, if 20x20 turns out too slow, is Blokus Duo (14x14, 2 colors,
13,729 placements against 30,433). It would need engine changes (board size,
start cells), so it isn't the first choice.

## Network

The network is a small ResNet in PyTorch. Its size is decided by measuring,
and these are the candidates:

| Name | Blocks x channels | MACs per eval | Guess at 50 GFLOPS |
|---|---|---|---|
| S | 6 x 32 | ~50M | ~500 evals/s |
| M | 6 x 64 | ~180M | ~140 evals/s |
| L | 10 x 64 | ~300M | ~80 evals/s |

### Canonical frame

Colors move clockwise from the top-left corner, so rotating the board by a
multiple of 90° puts the mover's start corner at the top left. The network
always sees the board that way, with the colors ordered mover, next,
partner, previous. Every color then shares the same weights and the same
opening. Moves are rotated back with the matching orientation from
`Tile::orientations()`.

### Inputs

**Planes** (12 x 20 x 20), each a set of 4, one per color in the order
above:
- occupied cells,
- live corners (slots that are still available, where that color's next
  tile can go),
- available cells (empty and not edge-adjacent to that color).

All of these are cheap bitmaps the `Board` already keeps.

**Global vector** (88 values):
- the tiles each color has left (4 x 21),
- whether each color can still move (4).

A linear layer turns the global vector into a per-channel bias added after
the stem, so the board convolutions know which tiles are left.

### Heads

**Policy:** a 1x1 convolution to 91 channels, giving one logit per
(orientation, top-left cell of its bounding box), 36,400 in all. Illegal
moves are masked before the softmax. There's no pass output, because the
rules only allow a pass when there are no moves.

**Value:** global average pooling, then an MLP to:
- win, draw and loss logits for the mover's player,
- the mover's player's score margin, scaled by 1/20, as an auxiliary target.

**Loss:** policy cross-entropy, plus value cross-entropy, plus a small weight
on the margin's Huber loss, plus L2.

## Search: Gumbel MCTS

The search follows Gumbel AlphaZero (Danihelka et al., "Policy improvement by
planning with Gumbel", ICLR 2022):

- **At the root**, it samples k ≈ 16 candidate moves with Gumbel top-k from
  the policy, then spends the simulations on them by sequential halving.
- **Inside the tree**, it uses the paper's deterministic selection.
- **The training policy target** is the improved policy from completed Q
  values, not raw visit counts.

The method still improves the policy at 16–64 simulations per move, where
plain AlphaZero with PUCT needs hundreds. That's the key property on a CPU.

Leaves get the network's value with no rollouts. Evaluation is batched: one
self-play process advances many games at once and gathers their leaves into
one batch of 64–256 positions per inference call. A batch costs much less per
position than single evaluations.

## Data and training loop

**One encoder, in C++.** Feature encoding lives only in C++. Self-play writes
the encoded positions, and Python never needs the game engine.

**The shard format** is fixed-size binary records that numpy reads with
`np.fromfile` and a structured dtype:
- planes as bits (12 x 400 bits = 600 bytes),
- the global vector,
- the legal-move mask, stored as the move indices,
- a sparse policy target (the top 32 moves, as uint16 index and float16
  probability),
- the outcome and the margin.

That's about 1 KB per position. A window of 1M positions is about 1 GB on
disk.

**The loop:**
- `zero:selfplay` (C++) writes shards and loads the newest ONNX model as it
  appears.
- `zero/train/train.py` trains on a sliding window of recent positions and
  exports ONNX every N steps.
- They run side by side, about 6 cores for self-play and 2 for training.

**The bootstrap** is supervised learning from today's MCTS. Self-play
between MCTS players at 1000 iterations, recording each move's root visit
distribution, gives about 1500 games (100k positions) an hour on 8 cores.
That's enough to train a first network that already plays sensibly, so RL
doesn't start from random play.

## Resource estimates

These are estimates, to be replaced by measurements after milestone 2.

**Self-play.** Assume about 70 moves per game and 32 simulations per move,
so about 2,200 evaluations per game:

| Net | Evals/s (guess) | Games per day | Positions per day |
|---|---|---|---|
| S | 500 | ~20k | ~1.4M |
| M | 140 | ~5k | ~400k |

**Training** costs about 3x the forward pass, so it keeps up easily. On 2
cores, S trains on several million samples a day.

**How much is needed:** probably 50k–200k self-play games to clearly beat
MCTS at 10k iterations. For S, that's days to a couple of weeks. This is
the least certain number in the plan.

## Software

- **Training:** PyTorch (CPU wheels), managed with `uv` (already on the
  VM). The code lives in `zero/train/`, with `pyproject.toml` and `uv.lock`
  checked in, outside Bazel. The venv is `~/venvs/blokuszero`, on the VM
  disk (`UV_PROJECT_ENVIRONMENT` points uv at it).
- **Inference:** ONNX Runtime's prebuilt CPU library, which Bazel fetches as
  an `http_archive` from the GitHub release and wraps in a `cc_library`.
  Nothing gets installed machine-wide. PyTorch exports with
  `torch.onnx.export`.
  - LibTorch was rejected: it's a much larger download, and its CPU
    inference for small nets is no faster.
  - Hand-written C++ inference stays possible later, if profiling shows ONNX
    Runtime overhead dominates at these sizes.
- **Checking the two sides agree:** a test feeds the same inputs to PyTorch
  and to the C++ evaluator, and compares the outputs.

## Milestones

Each milestone is one reviewed change, or a few, as usual.

1. **Python model.** A uv project with the configurable ResNet, ONNX export
   with random weights, and a shape test.
2. **C++ features and inference, then measure.**
   - `zero/features.{h,cc}`: canonical frame, input encoding, and Move ↔
     policy index mapping. Tested by round-tripping every legal move in
     random games, and by checking that rotating the frame and back is the
     identity.
   - `zero/evaluator.{h,cc}`: a batched ONNX Runtime session.
   - `zero:evaluator_benchmark`: evals/s by net size, batch size and thread
     count.

   **Decision point:** pick the net size, simulations per move, and whether
   20x20 is feasible.
3. **Supervised bootstrap.**
   - MctsAI exposes root visit counts.
   - `zero:generate` writes shards from MCTS self-play, and `train.py`
     trains on them.
   - A `zero` player in `main:match` plays with the network's policy alone
     (no search) as a first strength check.
4. **Gumbel search.** `zero/search.{h,cc}`, plus the batched multi-game
   `zero:selfplay`.
5. **RL loop.** A driver script, regular `main:match` evaluations against
   MCTS presets, and an Elo log in `analysis/blokuszero.md`.
6. **Web server.** BlokusZero presets.

## Decisions

- Prebuilt ONNX Runtime fetched by Bazel: OK.
- The venv is `~/venvs/blokuszero`, outside the shared project folder. uv
  may download its own Python if PyTorch doesn't support 3.14 yet.
- Data goes in `zero/data/` in the project folder (git-ignored).
