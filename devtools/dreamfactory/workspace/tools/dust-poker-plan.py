# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only test oracle for Dust's scripted hand scores and reserved draws."""
from collections import Counter
from itertools import combinations
import json
from pathlib import Path
import runpy
import sys

def score(cards):
    if len(cards) != 5:
        raise ValueError('Expected five cards')
    ranks = [14 if c == 'cheat' else {'a': 14, 'k': 13, 'q': 12, 'j': 11}.get(c[:-1], 0) or int(c[:-1]) for c in cards]
    suits = ['s' if c == 'cheat' else c[-1] for c in cards]
    counts = Counter(ranks)
    high = max(ranks)
    pairs = [r for r, n in counts.items() if n == 2]
    triples = [r for r, n in counts.items() if n == 3]
    quads = [r for r, n in counts.items() if n == 4]
    flush = len(set(suits)) == 1
    straight = len(counts) == 5 and min(ranks) >= high - 4
    # SALGAMES.FLT calcscore uses only this category/rank, with no kickers.
    if flush and straight: return 900 + high
    if quads: return 800 + quads[0]
    if triples and pairs: return 700 + triples[0]
    if flush: return 600 + high
    if straight: return 500 + high
    if triples: return 400 + triples[0]
    if len(pairs) == 2: return 300 + max(pairs)
    if pairs: return 200 + pairs[0]
    return high

if __name__ == '__main__':
    read_globals = runpy.run_path(str(Path(__file__).with_name('read-checkpoint-globals.py')))['read_globals']
    state = read_globals(sys.argv[1])
    hand = state['playerhand'].split()
    draws = state['playerhandtemp'].split()
    opponents = {name: score(state[name + 'hand'].split()) for name in ('mez', 'pete', 'zeb')}
    best = None
    for count in range(min(3, len(draws)) + 1):
        for indices in combinations(range(5), count):
            candidate = list(hand)
            for draw, index in enumerate(indices): candidate[index] = draws[draw]
            result = {'discards': [hand[i] for i in indices], 'hand': candidate, 'score': score(candidate)}
            if best is None or result['score'] > best['score']: best = result
    best['opponents'] = opponents
    best['winsWithoutFolds'] = best['score'] >= max(opponents.values())
    print(json.dumps(best))
