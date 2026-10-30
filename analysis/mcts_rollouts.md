# Better MCTS rollouts

October 2026. Can MCTS get stronger with smarter rollouts than uniformly
random moves? `main:match` results, 2-player, single thread, A's score with
a 95% CI. Results are in [`data/mcts_rollouts.txt`](data/mcts_rollouts.txt).

## Summary

- **Favoring larger tiles** (`RolloutOptions::size_bias`, beta) helps a lot
  at 1000 iterations: beta 1 or 2 scores 0.65 against uniform rollouts. At
  10k the gain mostly disappears: 0.52 ± 0.05 over 424 games, not
  significant. It costs nothing in speed.
- **Score-margin rewards** (`margin_weight` 0.5, `margin_scale` 10) help a
  little on top of that at 1000 iterations (0.57 ± 0.06 over 300 games), and
  make MCTS win by much larger margins. Also free.
- **Both together at 10k** (beta 1, `margin_weight` 0.5) beat the default
  0.585 ± 0.046 over 440 games (about +60 Elo), winning by 16 points on
  average. But beta 1 alone scores only 0.52 against the default, and
  margin rewards score 0.47 ± 0.065 against beta 1 alone, so the combined
  result is probably partly luck. Margin rewards make the wins bigger, not
  more frequent.
- **Best-of-k by corner heuristics** (`num_candidates`) makes MCTS clearly
  weaker, and slower. Greedier rollouts give worse value estimates. The
  code was removed afterwards.
- `size_bias` 1 is now the default; `margin_weight` stays 0.

## Favoring larger tiles

Each possible move in a rollout is weighted by `exp(beta * tile size)`, so
at beta 1 a pentomino placement is e^4 = 55 times as likely as a 1x1
placement. Uniform rollouts already lean towards large tiles, since 63 of
the 91 orientations are pentominoes, but they spend the small tiles as
readily as the big ones.

1000 iterations, against uniform rollouts (beta 0), 200 games each:

| A's beta | Wins-ties-losses | A's score |
|---|---|---|
| 0.5 | 110-4-86 | 0.56 ± 0.07 |
| 1 | 126-8-66 | 0.65 ± 0.07 |
| 2 | 127-6-67 | 0.65 ± 0.07 |
| 4 | 103-6-91 | 0.53 ± 0.07 |

10k iterations, against uniform rollouts:

| A's beta | Wins-ties-losses | A's score |
|---|---|---|
| 1 | 106-2-92 | 0.535 ± 0.07 |
| 1 (confirmation, new seeds) | 112-5-107 | 0.51 ± 0.065 |
| 1 (all, 424 games) | 218-7-199 | 0.52 ± 0.048 |
| 2 | 53-1-46 (stopped at 100 games) | 0.535 ± 0.10 |

At 10k, beta 1 is no worse than uniform and perhaps slightly better. The
mean margin in the confirmation was about 0. It is the default because it
clearly helps at low iteration counts and costs nothing.

Rollout speed is unchanged (`BM_Rollout` about 245 µs at beta 0, 1 and 4).
Too strong a bias (beta 4) loses most of the gain: the rollouts become
nearly deterministic about which tile to play.

## Best of k candidates

(Since removed, along with `Board::Effects`.) `num_candidates` k sampled k
moves with the size bias, and played the one
with the best `corner_weight * (corners gained - corners lost) +
block_weight * opponent corners covered` (`Board::Effects`). Corners are
the cells where a color can place its next tile.

1000 iterations, beta 1 on both sides, A adds candidates, 100 games each:

| A's candidates | A's score | Rollout cost |
|---|---|---|
| k = 2 | 0.46 ± 0.10 | +20% |
| k = 4 | 0.39 ± 0.09 | +50% |
| k = 8 | 0.31 ± 0.09 | +95% |
| k = 4, corners only (`block_weight` 0) | 0.23 ± 0.08 | +50% |
| k = 4, blocking only (`corner_weight` 0) | 0.38 ± 0.09 | +50% |

The more greedy the rollouts, the worse MCTS plays, even with the same
number of iterations. Chasing corners was the worst. This matches what is
known from Go: a rollout policy that plays better moves doesn't necessarily
give better win-rate estimates, because its mistakes are systematic rather
than random, so they don't average out (Silver and Tesauro, "Monte-Carlo
Simulation Balancing", 2009).

## Score-margin rewards

A rollout's reward mixes the win (1, or a share of a tie) with a reward for
the margin over the best other player:
`(1 - w) * win + w * (0.5 + 0.5 * tanh(margin / scale))`.

1000 iterations, beta 1 on both sides, A adds margin rewards, 100 games
each unless noted:

| A's `margin_weight`, `margin_scale` | A's score | A's mean margin |
|---|---|---|
| 0.25, 10 | 0.51 ± 0.10 | +9.0 |
| 0.5, 10 (300 games, three seeds) | 0.57 ± 0.06 | +13.6 |
| 1, 10 | 0.56 ± 0.10 | +13.8 |
| 0.5, 25 | 0.55 ± 0.10 | +14.9 |

The first 100 games at 0.5 and 10 scored 0.66, and two more seeds scored
0.515 and 0.525, so the first run was lucky. The win-rate gain is modest,
but every setting wins by a much bigger margin: MCTS keeps playing for
points when the result looks decided, instead of treating every move as
equal.

## Both together at 10k iterations

beta 1 and `margin_weight` 0.5 against the default (uniform rollouts, win
rewards only):

| Games | Wins-ties-losses | A's score | A's mean margin |
|---|---|---|---|
| 216 (first batch) | 114-8-94 | 0.55 ± 0.07 | +15 |
| 224 (second batch) | 139-1-84 | 0.62 ± 0.06 | +18 |
| 440 (all) | 253-9-178 | 0.585 ± 0.046 | +16 |

A spends about 2% more time per move.

Margin rewards on their own at 10k: beta 1 on both sides, A adds
`margin_weight` 0.5, 224 games:

| Wins-ties-losses | A's score | A's mean margin |
|---|---|---|
| 104-4-116 | 0.47 ± 0.065 | +16 |

No win-rate gain, but A still wins by 16 points on average: it wins big
and loses narrowly. Together with the 1000-iteration result (0.57 ± 0.06),
margin rewards are about neutral for winning. They may still be worth
having where the score matters, or to make MCTS play on in decided games.
