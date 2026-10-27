#!/usr/bin/env python3
"""Fits Elo ratings to main:match results.

Reads the machine-readable lines that main:match prints,
        result,<A name>,<B name>,<wins>,<ties>,<losses>
from the given files (or stdin), and fits a Bradley-Terry model to all of
them by maximum likelihood, counting a tie as half a win for each side.
Ratings are on the Elo scale (a 400-point gap means 10:1 odds) with the
anchor player at 0. Confidence intervals come from a parametric bootstrap
that resamples every match's games.

        python3 tools/elo.py --anchor=random results/*.txt

Each pair of players that met gets one extra virtual tie, so that a player
who won (or lost) every game still gets a finite rating. With 100-game
matches this moves ratings by a few points at most.
"""

import argparse
import collections
import math
import random
import sys


def read_results(files):
    """Returns {(a, b): [wins, ties, losses]} summed over all result lines."""
    results = collections.defaultdict(lambda: [0, 0, 0])
    for f in files:
        for line in f:
            if not line.startswith('result,'):
                continue
            _, a, b, wins, ties, losses = line.strip().split(',')
            if a == b:
                continue  # Self-play says nothing about ratings.
            if a > b:
                a, b = b, a
                wins, losses = losses, wins
            counts = results[(a, b)]
            counts[0] += int(wins)
            counts[1] += int(ties)
            counts[2] += int(losses)
    return results


def fit(results, anchor, iterations=10000, tolerance=1e-10):
    """Returns {player: Elo} fit by the Bradley-Terry MM algorithm."""
    players = sorted({p for pair in results for p in pair})
    # points[p]: p's total points, including the virtual ties.
    points = collections.defaultdict(float)
    # games[(a, b)]: games between a and b, including the virtual tie.
    games = {}
    for (a, b), (wins, ties, losses) in results.items():
        points[a] += wins + 0.5 * ties + 0.5
        points[b] += losses + 0.5 * ties + 0.5
        games[(a, b)] = wins + ties + losses + 1
    opponents = collections.defaultdict(list)
    for (a, b), n in games.items():
        opponents[a].append((b, n))
        opponents[b].append((a, n))

    strength = {p: 1.0 for p in players}
    for _ in range(iterations):
        new = {}
        for p in players:
            denominator = sum(n / (strength[p] + strength[q])
                              for q, n in opponents[p])
            new[p] = points[p] / denominator
        scale = new[anchor]
        new = {p: s / scale for p, s in new.items()}
        change = max(abs(math.log(new[p] / strength[p])) for p in players)
        strength = new
        if change < tolerance:
            break
    return {p: 400 * math.log10(s) for p, s in strength.items()}


def resample(results, rng):
    """Returns `results` with each match's games redrawn from its own rates."""
    resampled = {}
    for pair, counts in results.items():
        n = sum(counts)
        draws = rng.choices(range(3), weights=counts, k=n)
        resampled[pair] = [draws.count(i) for i in range(3)]
    return resampled


def expected_score(elo_a, elo_b):
    return 1 / (1 + 10 ** ((elo_b - elo_a) / 400))


def sort_key(player):
    """Sorts the random player first, then by MCTS iterations, then by name."""
    if player == 'random':
        return (0, 0, player)
    if player.startswith('mcts_i'):
        return (1, int(player[len('mcts_i'):].split('_')[0]), player)
    return (2, 0, player)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('files', nargs='*', type=argparse.FileType('r'),
                        help='main:match output (default: stdin)')
    parser.add_argument('--anchor', default='random',
                        help='player whose rating is fixed at 0')
    parser.add_argument('--bootstrap', type=int, default=1000,
                        help='bootstrap samples for the confidence intervals')
    parser.add_argument('--seed', type=int, default=0)
    args = parser.parse_args()

    results = read_results(args.files or [sys.stdin])
    if not results:
        sys.exit('No result lines found.')
    players = {p for pair in results for p in pair}
    if args.anchor not in players:
        sys.exit(f'Anchor {args.anchor} played no games.')

    elo = fit(results, args.anchor)
    rng = random.Random(args.seed)
    samples = collections.defaultdict(list)
    for _ in range(args.bootstrap):
        for p, rating in fit(resample(results, rng), args.anchor).items():
            samples[p].append(rating)

    print(f'{"player":<28} {"Elo":>6}  95% CI')
    for p in sorted(players, key=sort_key):
        ratings = sorted(samples[p])
        low = ratings[int(0.025 * len(ratings))] if ratings else elo[p]
        high = ratings[int(0.975 * len(ratings)) - 1] if ratings else elo[p]
        print(f'{p:<28} {elo[p]:6.0f}  [{low:.0f}, {high:.0f}]')

    print()
    print(f'{"A":<28} {"B":<28} {"games":>5} {"score":>6} {"fit":>6}')
    by_players = lambda item: (sort_key(item[0][0]), sort_key(item[0][1]))
    for (a, b), (wins, ties, losses) in sorted(results.items(), key=by_players):
        n = wins + ties + losses
        score = (wins + 0.5 * ties) / n
        fit_score = expected_score(elo[a], elo[b])
        print(f'{a:<28} {b:<28} {n:5d} {score:6.3f} {fit_score:6.3f}')


if __name__ == '__main__':
    main()
