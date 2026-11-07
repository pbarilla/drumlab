#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Common drum machine grooves, written for this repo (our own patterns; the idea of a page of them is after Pocket Operations by
Shittyrecording.studio). Used by the Drum patterns page (gen_site.py) and by the sample projects (make_samples.py).
A pattern: {sound 1-16: [steps 1-16]}, accents: [steps]. Sounds 1-4 are the top row of the pads: in every kit a kick, a snare, a hat and a
bell, tom or cowbell. Lane = (sound - 1) % 4."""

# name, bpm, swing (0-12), one line of what it is, hits, accents
PATTERNS = [
    ("Four on the floor", 124, 0, "Kick on every beat, hat in between. House, techno, disco.",
     {1: [1, 5, 9, 13], 3: [3, 7, 11, 15]}, []),
    ("Backbeat", 110, 0, "Snare on 2 and 4. Rock, pop, most things.",
     {1: [1, 9, 11], 2: [5, 13], 3: [1, 3, 5, 7, 9, 11, 13, 15]}, [5, 13]),
    ("Boom bap", 90, 4, "A lazy kick behind the snare. Old hip hop.",
     {1: [1, 8, 11], 2: [5, 13], 3: [1, 3, 5, 7, 9, 11, 13, 15]}, [5, 13]),
    ("Offbeat hats", 126, 0, "Kick, snare, and the hat on the offbeats.",
     {1: [1, 5, 9, 13], 2: [5, 13], 3: [3, 7, 11, 15], 4: [16]}, [5, 13]),
    ("Electro", 128, 0, "A kick that skips around, snare on 2 and 4.",
     {1: [1, 4, 9, 12], 2: [5, 13], 3: [3, 7, 11, 15], 4: [8, 16]}, [5, 13]),
    ("Hat roll", 140, 0, "Sparse kick and snare, then the hat speeds up at the end.",
     {1: [1, 11], 2: [9], 3: [1, 3, 5, 7, 9, 11, 13, 14, 15, 16]}, [9]),
    ("Breakbeat", 100, 0, "A kick that doesn't wait for the beat.",
     {1: [1, 4, 7, 11], 2: [5, 13], 3: [1, 3, 5, 7, 9, 11, 13, 15], 4: [16]}, [5, 13]),
    ("Half-time", 140, 0, "The snare moves to beat 3, so everything feels twice as slow.",
     {1: [1, 8, 12], 2: [9], 3: [1, 3, 5, 7, 9, 11, 13, 15]}, [9]),
    ("Two-step", 132, 6, "Kick on 1 and the 'and' of 3, snare on 2 and 4, shuffled. Garage.",
     {1: [1, 11], 2: [5, 13], 3: [1, 3, 5, 7, 9, 11, 13, 15]}, [5, 13]),
    ("One drop", 76, 0, "Nothing on beat 1. Kick and snare together on 3. Reggae.",
     {1: [9], 2: [9], 3: [1, 3, 5, 7, 9, 11, 13, 15]}, [9]),
    ("Trap", 140, 0, "Half-time, with the hat rolling at the end of the bar.",
     {1: [1, 7, 12], 2: [9], 3: [1, 3, 5, 7, 9, 11, 13, 15, 16], 4: [15]}, [9]),
    ("Shuffle", 96, 10, "Swing all the way up and the same plain groove turns into a skip.",
     {1: [1, 9], 2: [5, 13], 3: [1, 3, 5, 7, 9, 11, 13, 15]}, [5, 13]),
]

# the sample projects: name, kit ('LAB-1' or one of the preset kits), the patterns it holds (by name or hits) and the chain (0-based)
SAMPLES = [
    ("Four on the floor", "TUBE", ["Four on the floor", "Offbeat hats", "Electro"], [0, 0, 1, 1, 0, 0, 1, 2]),
    ("Breakbeat", "DUSTY", ["Breakbeat", "Backbeat", "Hat roll"], [0, 0, 1, 0, 0, 1, 2]),
    ("Half-time", "SPACE", ["Half-time", "Trap", "One drop"], [0, 0, 1, 0, 0, 1, 2]),
    ("Two-step", "LAB-1", ["Two-step", "Shuffle", "Boom bap"], [0, 0, 1, 0, 0, 1, 2]),
]


def by_name(n):
    for p in PATTERNS:
        if p[0] == n:
            return p
    raise KeyError(n)


def trig(p):
    """-> 4 x 16 trig bytes as the device keeps them (1 | row << 4) and the accent mask"""
    t = [[0] * 16 for _ in range(4)]
    for s, steps in p[4].items():
        for st in steps:
            t[(s - 1) & 3][st - 1] = 1 | (((s - 1) >> 2) << 4)
    a = 0
    for st in p[5]:
        a |= 1 << (st - 1)
    return t, a
