# How MCTS strength scales with iterations

October 2026, at commit 379909d (after the move-choice tie-break fix).

## Question

How much stronger does `MctsAI` get as `num_iterations` grows, and does the
gain from each extra factor of iterations shrink? The answer is a single Elo
scale with the random player at 0, so that later changes (a different `c`,
better rollouts, BlokusZero) can be rated against it.

## Setup

- Levels: `RandomAI`, then MCTS at 10, 30, 100, 300, 1000, 3000, 10k, 30k
  and 100k iterations per move, so each step is about 3x.
- MCTS config: `c = 0.25`, 1 thread (so virtual loss is unused), 1 rollout
  per iteration. These are the defaults apart from the thread count. The
  player names in the data encode this, e.g. `mcts_i1000_c0.25_t1_vl3`.
- Pairings: each level against the next level and the one after that, for
  17 pairings. Each pairing is 104 2-player games from `main:match`, in 4
  shards of 26 games run in parallel. Seats alternate, so each side plays
  first in half the games.
- Elo: `tools/elo.py` fits a Bradley-Terry model to all 17 pairings at once,
  counting ties as half a win. The 95% CIs come from 1000 bootstrap
  resamples of the games.
- Raw results: [`data/mcts_iterations.txt`](data/mcts_iterations.txt).
  Reproduce the table with `python3 tools/elo.py
  analysis/data/mcts_iterations.txt`.

The whole run took about 11.5 hours on the 4-vCPU VM.

## Results

| Iterations | Elo | 95% CI | Gain over previous level | Time per game |
|---|---|---|---|---|
| Random | 0 | | | 0 |
| 10 | 301 | 237-386 | +301 | 0.04 s |
| 30 | 417 | 342-506 | +116 | 0.13 s |
| 100 | 630 | 544-737 | +213 | 0.43 s |
| 300 | 876 | 785-994 | +246 | 1.3 s |
| 1000 | 1312 | 1191-1485 | +436 | 4.2 s |
| 3000 | 1780 | 1630-1996 | +468 | 12.5 s |
| 10k | 2163 | 1999-2406 | +383 | 44 s |
| 30k | 2328 | 2157-2577 | +165 | 134 s |
| 100k | 2521 | 2348-2784 | +193 | 472 s |

"Time per game" is one side's total time in `SelectMove` over a game (about
40 moves), single-threaded. It's linear in iterations, at about 4.5 ms per
iteration per game.

The CIs widen up the ladder because each rating builds on the ones below it.
The gaps between neighbouring levels are better determined than the
absolute ratings suggest: each comes from two or three direct pairings.

Pairings (A's score, and the score the fitted ratings predict):

| A | B | A's score | Fit |
|---|---|---|---|
| 10 | random | 0.880 | 0.850 |
| 30 | random | 0.894 | 0.917 |
| 30 | 10 | 0.692 | 0.661 |
| 100 | 10 | 0.870 | 0.869 |
| 100 | 30 | 0.745 | 0.773 |
| 300 | 30 | 0.971 | 0.934 |
| 300 | 100 | 0.769 | 0.805 |
| 1000 | 100 | 0.990 | 0.981 |
| 1000 | 300 | 0.933 | 0.925 |
| 3000 | 300 | 0.990 | 0.995 |
| 3000 | 1000 | 0.957 | 0.937 |
| 10k | 1000 | 0.990 | 0.993 |
| 10k | 3000 | 0.894 | 0.901 |
| 30k | 3000 | 0.981 | 0.959 |
| 30k | 10k | 0.707 | 0.721 |
| 100k | 10k | 0.889 | 0.887 |
| 100k | 30k | 0.755 | 0.752 |

## Observations

- **One Elo scale fits well.** Every pairing's score is within about 0.04
  of what the fitted ratings predict, which is inside the noise of a
  104-game match. The skip-one pairings (e.g. 1000 vs 10k) agree with the
  chains of adjacent ones (1000 vs 3000 vs 10k).
- **The gain per 3x is not constant.** It is about 120-250 Elo up to 300
  iterations, rises to about 450 between 300 and 3000, and falls back to
  about 170-190 from 10k on.
- **Low budgets barely search.** Positions have hundreds of legal moves, so
  below about 1000 iterations most root moves get one visit or none, and the
  choice is close to picking the best of a few random rollouts. Between 300
  and 3000 the visits start to concentrate on the better moves, and strength
  climbs fastest.
- **High budgets give diminishing returns.** Above 10k each 3x buys under
  200 Elo while costing 3x the time. Two likely limits are random-rollout
  evaluations, which stay noisy however many there are, and `c = 0.25`,
  which was tuned at 10k iterations and may explore too little for 100k-node
  trees.
- **Even 10 iterations is far better than random** (88% of games). A handful
  of rollouts is probably enough to avoid the worst moves, such as ones that
  leave a color few corners to play from.

## Caveats

- All levels use `c = 0.25`. The `c` sweep found the best `c` grows with the
  budget, so the low levels would gain a little from smaller `c` and 100k
  might gain from larger `c`. These ratings are for the default config at
  each budget, not for each budget at its best.
- 2-player only. Elo is a two-player measure, and 4-player strength needs a
  different method.
- Pairings with scores above about 0.97 (30 vs 300, 100 vs 1000, 300 vs
  3000, 1000 vs 10k, 3000 vs 30k) carry little information on their own.
  The fit leans on the closer pairings.

## Follow-ups

- Rate other configs against this ladder by matching them against the 2-3
  closest levels and refitting with all the data, e.g. `c = 0.5` at 100k.
- Tune `c` at 100k, where the binaries' strong AI plays.
- Smarter rollouts or a value function (BlokusZero) should show up as a jump
  at a fixed budget.
