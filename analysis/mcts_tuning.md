# Tuning MCTS: `c`, threads, virtual loss, move choice

October 2026. `main:match` results, 2-player, single thread unless noted,
A's score with a 95% CI. 100 games per match unless noted.

## Summary

- `c = 1.4` (sqrt(2), the textbook value) explores far too much. The best
  `c` grows with the iteration budget: about 0-0.1 at 1000 iterations, and
  0.25 at 10k. The default is `c = 0.25`.
- With `c` tuned, 4 threads play as well as 1 thread at 10k iterations,
  2.8x faster. Virtual loss is neutral to slightly helpful, and defaults to
  3.
- Breaking move-choice ties by the first child made low-budget MCTS much
  weaker. It's fixed, but the 1000-iteration `c` sweep below predates the
  fix.

## Before tuning (`c = 1.4`, 1000 iterations)

At commit 7f84395, before tied rollouts split the reward. Unless noted, both
sides use `c = 1.4` and 1000 iterations.

| A vs B | A's score |
|---|---|
| Identical configs | 0.54 ± 0.10 (no meaningful seat bias) |
| 1000 vs 250 iterations | 0.85 ± 0.07 |
| 4 threads vs 1 thread | 0.66 ± 0.09 |
| 4 threads with virtual loss 1 vs 1 thread | 0.43 ± 0.10 |
| 4 threads with virtual loss 3 vs 1 thread | 0.39 ± 0.10 |
| `c = 0.7` vs 1.4 | 0.62 ± 0.09 |
| `c = 0.35` vs 1.4 | 0.89 ± 0.06 |

`c = 1.4` explores far too much. Threads piling onto the same leaf helped
because they exploit more, and virtual loss undid that.

## Sweeping `c`

At commit 5bbaf1f (ties split the reward), before the move-choice fix.

1000 iterations, against `c = 0.35`:

| A's `c` | Games | A's score |
|---|---|---|
| 0 | 200 | 0.59 ± 0.07 |
| 0.05 | 200 | 0.56 ± 0.07 |
| 0.1 | 200 | 0.57 ± 0.07 |
| 0.2 | 100 | 0.53 ± 0.10 |
| 0.5 | 100 | 0.34 ± 0.09 |

10k iterations, against `c = 0.1`:

| A's `c` | Games | A's score |
|---|---|---|
| 0 | 100 | 0.23 ± 0.08 |
| 0.15 | 100 | 0.45 ± 0.10 |
| 0.25 | 200 (two seeds) | 0.58 ± 0.07 |
| 0.35 | 100 | 0.45 ± 0.10 |
| 0.5 | 100 | 0.28 ± 0.09 |

Direct comparisons:
- 10k iterations, `c = 0.25` vs 0.35: 0.61 ± 0.07 (198 games).
- 20k iterations, `c = 0.3` vs 0.15: 0.46 ± 0.10 (no clear difference).

The best `c` grows with the budget. At 1000 iterations pure exploitation
(`c = 0`) is as good as anything, but at 10k it loses badly. `c = 0.25`
became the default, tuned for the 10k-20k budgets the binaries use. The
strong web AI plays at 200k, where the best `c` is probably higher; that is
too slow to tune with `main:match` on the 4-vCPU VM.

## Threads and virtual loss at `c = 0.25`

10k iterations, at commit c050c8b.

| A vs B | A's score | Time in `SelectMove`, A / B |
|---|---|---|
| 4 threads vs 1 thread, no virtual loss | 0.49 ± 0.10 | 17 min / 48 min |
| 4 threads each, virtual loss 1 vs none | 0.51 ± 0.10 | 21 min / 21 min |
| 4 threads each, virtual loss 3 vs none | 0.53 ± 0.10 | 23 min / 23 min |

The advantage threads had at `c = 1.4` came from exploiting more, and is
gone now that `c` is tuned: 4 threads match 1 thread at 2.8x the speed.
Neither virtual-loss result is significant, but the trend is upward, so the
default became 3.

## Move-choice tie-break

`SelectMove` plays the most-visited root child. It used to take the first
one on a tie. Children are in tile order, so with fewer iterations than
moves it played the smallest tile it had tried. Commit 379909d breaks ties
by total reward, then randomly.

| Match | Old tie-break's score |
|---|---|
| Old vs new, 1000 iterations (104 games) | 0.37 ± 0.09 |
| Old vs new, 10k iterations (104 games) | 0.50 ± 0.10 |

Random against MCTS at 30 iterations (40 games) scored 0.60 against the old
tie-break and 0.05 against the new one.

## Follow-ups

- Re-sweep `c` at 1000 iterations with the tie-break fix.
- Tune `c` at 100k-200k iterations, where the strong web AI plays.
- Retest virtual loss with more threads, on a machine with more cores.
