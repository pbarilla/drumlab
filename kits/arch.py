#!/usr/bin/env python3
"""Our drum archetypes and preset kits, written in the units Microtonic shows (Hz, ms, dB, %).
32 archetypes: 8 for each column of the PO-32's 4 x 4 keys (a column is one voice: sound n sits in column n % 4, row n // 4).
  column 1  low      kicks, sub, a tom, a bass
  column 2  snare    snares, rim, clap, snap
  column 3  hats     closed / open hats, shaker, ride, crash, hiss
  column 4  perc     bell, hi tom, cowbell, conga, wood, zap, blip, tambourine
Each has an A patch and a B patch (what the PO-32's knob B morphs to). B comes from the archetype's own rule unless given.
`python3 kits/arch.py OUTDIR` writes OUTDIR/ccNN-name-A.mtdrum / -B.mtdrum (cc = column, NN = archetype); kits/mkarch.sh turns them into
firmware/gen/arch.h with libpo's importer (MIT, not bundled). The kits at the bottom are recipes: which archetype in each of the 16 places,
plus how the whole kit is tuned. Never bundle anyone else's presets."""
import sys
from pathlib import Path

DEF = dict(OscWave="Sine", OscFreq=100, OscAtk=0, OscDcy=200, ModMode="Decay", ModRate=60, ModAmt=0,
           NFilMod="LP (low-pass)", NFilFrq=8000, NFilQ=0.7, NEnvMod="Exponential", NEnvAtk=0, NEnvDcy=100,
           Mix=0, DistAmt=0, EQFreq=1000, EQGain=0, Level=-3, OscVel=100, NVel=100, ModVel=0)
FILT = {"LP": "LP (low-pass)", "BP": "BP (band-pass)", "HP": "HP (hi-pass)"}
ENV = {"exp": "Exponential", "lin": "Linear", "mod": "Modulate"}

def S(**k):
    d = dict(DEF); d.update(k)
    d["NFilMod"] = FILT.get(d["NFilMod"], d["NFilMod"]); d["NEnvMod"] = ENV.get(d["NEnvMod"], d["NEnvMod"])
    return d
def B(a, **k):                      # the morph target: the A patch with some changes
    d = dict(a); d.update(k)
    return d
def longer(a, dcy=1.8, dist=22, freq=0.92, noise=1.9, mix=6, lvl=-1):   # the usual B: bigger, longer, dirtier
    return B(a, OscFreq=round(a["OscFreq"] * freq), OscDcy=round(a["OscDcy"] * dcy), NEnvDcy=round(a["NEnvDcy"] * noise),
             DistAmt=min(80, a["DistAmt"] + dist), Mix=min(98, a["Mix"] + mix), Level=a["Level"] + lvl)

# ---------------------------------------------------------------- column 1: low
K0 = [
 ("boom",  S(OscFreq=54, OscDcy=640, ModAmt=36, ModRate=42, DistAmt=6, Mix=3, NFilFrq=400, NEnvDcy=10, Level=0, OscVel=60, ModVel=40)),
 ("punch",  S(OscFreq=58, OscDcy=300, ModAmt=46, ModRate=26, DistAmt=24, Mix=9, NFilFrq=1800, NEnvDcy=12, Level=0, OscVel=60, ModVel=40)),
 ("tight",S(OscFreq=72, OscDcy=140, ModAmt=34, ModRate=16, DistAmt=14, Mix=7, NFilFrq=2400, NEnvDcy=9, Level=0, OscVel=60, ModVel=40)),
 ("sub",  S(OscFreq=38, OscDcy=900, ModAmt=18, ModRate=90, DistAmt=0, Mix=0, Level=1, ModVel=30)),
 ("crunch",S(OscFreq=62, OscDcy=260, ModAmt=40, ModRate=24, DistAmt=52, Mix=12, NFilFrq=2200, NEnvDcy=14, Level=-2, OscVel=60, ModVel=40)),
 ("lo tom",    S(OscFreq=96, OscDcy=380, ModAmt=22, ModRate=60, Mix=4, NFilFrq=1200, NEnvDcy=40, Level=-1, ModVel=30)),
 ("bass",S(OscWave="Saw", OscFreq=55, OscDcy=520, ModAmt=2, ModRate=40, NFilMod="LP", NFilFrq=240, NFilQ=2.4, NEnvDcy=300, Mix=22, DistAmt=36, Level=-4)),
 ("thump",  S(OscFreq=64, OscDcy=260, ModMode="Sine", ModRate=130, ModAmt=9, DistAmt=18, Mix=4, NFilFrq=900, NEnvDcy=20, Level=-1, ModVel=30)),
]
# ---------------------------------------------------------------- column 2: snare
K1 = [
 ("crack", S(OscWave="Triangle", OscFreq=185, OscDcy=95, ModAmt=14, ModRate=24, NFilMod="HP", NFilFrq=2200, NFilQ=0.9, NEnvDcy=190, Mix=62, DistAmt=14, Level=-1, NVel=70)),
 ("soft", S(OscWave="Triangle", OscFreq=170, OscDcy=130, ModAmt=10, ModRate=30, NFilMod="BP", NFilFrq=3200, NFilQ=0.7, NEnvDcy=230, Mix=55, DistAmt=6, Level=-1, NVel=70)),
 ("tight",S(OscFreq=240, OscDcy=60, ModAmt=10, ModRate=20, NFilMod="BP", NFilFrq=3100, NFilQ=0.6, NEnvDcy=140, Mix=72, DistAmt=30, Level=-2)),
 ("rim",       S(OscWave="Triangle", OscFreq=1750, OscDcy=22, ModAmt=-8, ModRate=24, NFilMod="BP", NFilFrq=4100, NFilQ=2, NEnvDcy=24, Mix=40, DistAmt=24, Level=-4)),
 ("clap",      S(OscFreq=900, OscDcy=18, NFilMod="BP", NFilFrq=1500, NFilQ=2.2, NEnvMod="mod", NEnvDcy=240, Mix=94, DistAmt=18, Level=-3)),
 ("fm snr",  S(OscFreq=210, OscDcy=130, ModMode="Sine", ModRate=300, ModAmt=22, NFilMod="HP", NFilFrq=3000, NFilQ=1.2, NEnvDcy=150, Mix=44, DistAmt=18, Level=-3)),
 ("long",S(OscWave="Triangle", OscFreq=200, OscDcy=150, ModAmt=12, ModRate=30, NFilMod="HP", NFilFrq=1800, NFilQ=1.0, NEnvDcy=420, Mix=70, DistAmt=20, Level=-2, NVel=70)),
 ("snap",      S(OscWave="Triangle", OscFreq=330, OscDcy=30, ModAmt=18, ModRate=12, NFilMod="HP", NFilFrq=4200, NFilQ=1.4, NEnvDcy=70, Mix=60, DistAmt=26, Level=-3)),
]
# ---------------------------------------------------------------- column 3: hats and noise
K2 = [
 ("hat",S(OscWave="Saw", OscFreq=7800, OscDcy=18, NFilMod="HP", NFilFrq=7400, NFilQ=2.2, NEnvDcy=42, Mix=88, EQFreq=9500, EQGain=6, Level=-6, NVel=60)),
 ("tick",  S(OscWave="Saw", OscFreq=9000, OscDcy=10, NFilMod="HP", NFilFrq=8200, NFilQ=2.6, NEnvDcy=22, Mix=90, EQFreq=10500, EQGain=5, Level=-6, NVel=60)),
 ("open",  S(OscWave="Saw", OscFreq=7200, OscDcy=120, NFilMod="HP", NFilFrq=6900, NFilQ=1.8, NEnvDcy=360, Mix=90, EQFreq=10000, EQGain=5, Level=-7)),
 ("pedal", S(OscWave="Saw", OscFreq=6600, OscDcy=40, NFilMod="HP", NFilFrq=6200, NFilQ=1.6, NEnvDcy=110, Mix=88, EQFreq=9000, EQGain=4, Level=-6)),
 ("shaker",    S(OscWave="Saw", OscFreq=6200, OscDcy=20, NFilMod="BP", NFilFrq=5600, NFilQ=0.9, NEnvMod="lin", NEnvAtk=30, NEnvDcy=120, Mix=96, Level=-7, NVel=80)),
 ("ride",      S(OscWave="Saw", OscFreq=6200, OscDcy=300, ModMode="Sine", ModRate=3100, ModAmt=6, NFilMod="HP", NFilFrq=5200, NFilQ=1.4, NEnvDcy=900, Mix=70, EQFreq=9000, EQGain=4, Level=-8)),
 ("crash",     S(OscWave="Saw", OscFreq=5200, OscDcy=260, ModMode="Noise", ModRate=2400, ModAmt=14, NFilMod="HP", NFilFrq=4700, NFilQ=1.5, NEnvDcy=1600, Mix=80, EQFreq=8000, EQGain=6, Level=-8)),
 ("hiss",      S(OscWave="Saw", OscFreq=5000, OscDcy=10, NFilMod="HP", NFilFrq=3600, NFilQ=0.8, NEnvDcy=260, Mix=98, Level=-8, NVel=70)),
]
# ---------------------------------------------------------------- column 4: percussion
K3 = [
 ("bell",      S(OscWave="Triangle", OscFreq=585, OscDcy=210, ModMode="Sine", ModRate=545, ModAmt=9, NFilMod="BP", NFilFrq=2640, NFilQ=3, NEnvDcy=70, Mix=18, DistAmt=14, Level=-5)),
 ("hi tom",    S(OscFreq=190, OscDcy=260, ModAmt=18, ModRate=70, Mix=3, NFilFrq=2400, NEnvDcy=24, Level=-2, ModVel=30)),
 ("cowbell",   S(OscWave="Triangle", OscFreq=560, OscDcy=120, ModMode="Sine", ModRate=845, ModAmt=7, NFilMod="BP", NFilFrq=1700, NFilQ=2.4, NEnvDcy=40, Mix=10, DistAmt=18, Level=-5)),
 ("conga",     S(OscFreq=310, OscDcy=150, ModAmt=8, ModRate=24, NFilMod="BP", NFilFrq=2000, NFilQ=1.2, NEnvDcy=30, Mix=8, Level=-3, ModVel=30)),
 ("wood",      S(OscWave="Triangle", OscFreq=1100, OscDcy=26, ModAmt=-4, ModRate=14, NFilMod="BP", NFilFrq=2600, NFilQ=2.4, NEnvDcy=18, Mix=14, Level=-4)),
 ("zap",       S(OscWave="Saw", OscFreq=2400, OscDcy=210, ModAmt=-70, ModRate=70, DistAmt=22, Mix=10, Level=-8)),
 ("blip",      S(OscWave="Sine", OscFreq=880, OscDcy=70, ModAmt=12, ModRate=40, Mix=0, DistAmt=0, Level=-5)),
 ("tamb",S(OscWave="Saw", OscFreq=5600, OscDcy=60, ModMode="Sine", ModRate=2900, ModAmt=5, NFilMod="BP", NFilFrq=6400, NFilQ=1.6, NEnvMod="mod", NEnvDcy=300, Mix=88, EQFreq=9000, EQGain=4, Level=-7)),
]
ARCH = [K0, K1, K2, K3]
# a B patch that is not the usual "longer and dirtier"
BRULE = {(0, 3): dict(dcy=1.5, dist=14), (2, 0): dict(dcy=2.0, noise=2.4, dist=0), (2, 1): dict(dcy=2.0, noise=2.6, dist=0), (2, 4): dict(dcy=1.6, noise=2.2, dist=0), (2, 5): dict(dcy=1.4, noise=1.3, dist=0), (2, 6): dict(dcy=1.4, noise=1.25, dist=0), (2, 2): dict(dcy=1.8, noise=1.6, dist=0),
         (3, 5): dict(dcy=2.0, dist=36), (3, 6): dict(dcy=1.8, dist=30), (3, 0): dict(dcy=1.6, dist=18)}

def pair(c, i):
    a = ARCH[c][i][1]
    r = dict(dcy=1.8, dist=22, freq=0.92, noise=1.9, mix=6, lvl=-1); r.update(BRULE.get((c, i), {}))
    return a, longer(a, **r)

def text(d):
    return (f"OscWave: {d['OscWave']}\nOscFreq: {d['OscFreq']} Hz\nOscAtk: {d['OscAtk']} ms\nOscDcy: {d['OscDcy']} ms\n"
            f"ModMode: {'Noise FM' if d['ModMode']=='Noise' else d['ModMode']}\nModRate: {d['ModRate']} {'ms' if d['ModMode']=='Decay' else 'Hz'}\nModAmt: {d['ModAmt']} st\n"
            f"NFilMod: {d['NFilMod']}\nNFilFrq: {d['NFilFrq']} Hz\nNFilQ: {d['NFilQ']}\nNEnvMod: {d['NEnvMod']}\nNEnvAtk: {d['NEnvAtk']} ms\nNEnvDcy: {d['NEnvDcy']} ms\n"
            f"Mix: {100-d['Mix']}% osc, {d['Mix']}% noise\nDistAmt: {d['DistAmt']}%\nEQFreq: {d['EQFreq']} Hz\nEQGain: {d['EQGain']} dB\nLevel: {d['Level']} dB\n"
            f"OscVel: {d['OscVel']}%\nNVel: {d['NVel']}%\nModVel: {d['ModVel']}%\n")

# ---------------------------------------------------------------- the kits: which archetype sits in each of the 16 places (row r, column c = sound 4r + c)
# picks[c] = the four archetypes of column c, top row first. mods: tune (octaves), decay (x), dist (+ %), noise (+ %), the kit's character.
KITS = [
 ("TUBE",    [[0, 5, 3, 6], [1, 3, 4, 7], [0, 2, 4, 5], [0, 1, 2, 3]], dict(tune=0.0, decay=1.0, dist=0, noise=0)),
 ("BOX",     [[1, 2, 5, 4], [0, 3, 4, 7], [1, 2, 3, 6], [1, 2, 3, 7]], dict(tune=0.0, decay=0.9, dist=6, noise=0)),
 ("DUSTY",   [[0, 3, 7, 6], [6, 3, 5, 1], [3, 2, 7, 5], [3, 4, 2, 0]], dict(tune=-0.1, decay=1.1, dist=14, noise=6)),
 ("TOYBOX",  [[7, 5, 2, 6], [5, 3, 7, 4], [1, 3, 4, 7], [0, 6, 2, 4]], dict(tune=0.35, decay=0.8, dist=0, noise=-4)),
 ("ELECTRO", [[4, 2, 0, 3], [2, 5, 7, 4], [1, 0, 2, 6], [5, 6, 1, 2]], dict(tune=0.1, decay=0.85, dist=10, noise=0)),
 ("SPACE",   [[3, 7, 5, 6], [5, 6, 7, 3], [5, 6, 2, 7], [5, 0, 6, 1]], dict(tune=0.2, decay=1.4, dist=4, noise=0)),
 ("POCKET",  [[2, 5, 1, 6], [7, 3, 4, 0], [1, 3, 0, 4], [4, 6, 7, 1]], dict(tune=0.3, decay=0.7, dist=0, noise=0)),
 ("ACID",    [[1, 6, 3, 7], [4, 0, 3, 7], [0, 2, 1, 4], [2, 6, 5, 4]], dict(tune=0.0, decay=1.0, dist=14, noise=0)),
 ("CHIP",    [[2, 6, 5, 7], [7, 2, 3, 6], [1, 0, 7, 4], [6, 4, 5, 1]], dict(tune=0.15, decay=0.65, dist=0, noise=0)),
]

if __name__ == "__main__":
    out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
    for c in range(4):
        for i, (name, _) in enumerate(ARCH[c]):
            a, b = pair(c, i)
            for tag, d in (("A", a), ("B", b)):
                (out / f"{c}{i}-{name.replace(' ', '_')}-{tag}.mtdrum").write_text(text(d))
    print(sum(len(k) for k in ARCH), "archetypes ->", out)
