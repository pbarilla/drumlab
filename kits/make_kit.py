#!/usr/bin/env python3
"""Our own kit: 16 sounds, A and B patch each, written in the units Microtonic shows (Hz, ms, dB, %).
Writes kits/<name>/NN-name-A.mtdrum / -B.mtdrum; kit_header.sh turns them into firmware/gen/kit_lab1.h with libpo's importer.
Sound n sits in column (n-1) % 4 of the grid: a column is one voice, a new hit cuts the old, so a column holds sounds that never need to overlap."""
import sys, os
from pathlib import Path

DEF = dict(OscWave="Sine", OscFreq=100, OscAtk=0, OscDcy=200, ModMode="Decay", ModRate=60, ModAmt=0,
           NFilMod="LP (low-pass)", NFilFrq=8000, NFilQ=0.7, NEnvMod="Exponential", NEnvAtk=0, NEnvDcy=100,
           Mix=0, DistAmt=0, EQFreq=1000, EQGain=0, Level=-3, OscVel=100, NVel=100, ModVel=0)
FILT = {"LP": "LP (low-pass)", "BP": "BP (band-pass)", "HP": "HP (hi-pass)"}
ENV = {"exp": "Exponential", "lin": "Linear", "mod": "Modulate"}

def S(**k):
    d = dict(DEF); d.update(k)
    for key in ("NFilMod",):
        d[key] = FILT.get(d[key], d[key])
    d["NEnvMod"] = ENV.get(d["NEnvMod"], d["NEnvMod"])
    return d

# name, A, B (B = the morph target: the knob B on the PO-32 slides from A to B)
KIT = [
 ("kick",     S(OscFreq=52, OscDcy=330, ModAmt=44, ModRate=38, DistAmt=8, Mix=6, NFilFrq=500, NEnvDcy=14, Level=1, OscVel=60, ModVel=40),
              S(OscFreq=46, OscDcy=620, ModAmt=52, ModRate=60, DistAmt=34, Mix=8, NFilFrq=700, NEnvDcy=20, Level=0, OscVel=60, ModVel=40)),
 ("snare",    S(OscWave="Triangle", OscFreq=185, OscDcy=95, ModAmt=14, ModRate=24, NFilMod="HP", NFilFrq=2200, NFilQ=0.9, NEnvDcy=190, Mix=62, DistAmt=14, Level=-1, NVel=70),
              S(OscWave="Triangle", OscFreq=210, OscDcy=140, ModAmt=18, ModRate=30, NFilMod="HP", NFilFrq=1700, NFilQ=1.4, NEnvDcy=330, Mix=70, DistAmt=40, Level=-2, NVel=70)),
 ("hat",      S(OscWave="Saw", OscFreq=7800, OscDcy=18, NFilMod="HP", NFilFrq=7400, NFilQ=2.2, NEnvDcy=42, Mix=88, EQFreq=9500, EQGain=6, Level=-6, NVel=60),
              S(OscWave="Saw", OscFreq=8600, OscDcy=26, NFilMod="HP", NFilFrq=6200, NFilQ=3.5, NEnvDcy=95, Mix=92, EQFreq=9500, EQGain=8, Level=-6, NVel=60)),
 ("bell",  S(OscWave="Triangle", OscFreq=585, OscDcy=210, ModMode="Sine", ModRate=545, ModAmt=9, NFilMod="BP", NFilFrq=2640, NFilQ=3, NEnvDcy=70, Mix=18, DistAmt=14, Level=-5),
              S(OscWave="Triangle", OscFreq=800, OscDcy=330, ModMode="Sine", ModRate=720, ModAmt=14, NFilMod="BP", NFilFrq=3300, NFilQ=4, NEnvDcy=120, Mix=24, DistAmt=30, Level=-6)),
 ("sub", S(OscFreq=41, OscDcy=520, ModAmt=22, ModRate=42, Mix=0, DistAmt=0, Level=1, ModVel=30),
              S(OscFreq=36, OscDcy=900, ModAmt=30, ModRate=54, Mix=0, DistAmt=18, Level=0, ModVel=30)),
 ("snr 2",  S(OscFreq=240, OscDcy=60, ModAmt=10, ModRate=20, NFilMod="BP", NFilFrq=3100, NFilQ=0.6, NEnvDcy=140, Mix=72, DistAmt=30, Level=-2),
              S(OscFreq=300, OscDcy=80, ModAmt=12, ModRate=26, NFilMod="BP", NFilFrq=2500, NFilQ=0.9, NEnvDcy=260, Mix=78, DistAmt=52, Level=-3)),
 ("open", S(OscWave="Saw", OscFreq=7200, OscDcy=120, NFilMod="HP", NFilFrq=6900, NFilQ=1.8, NEnvDcy=360, Mix=90, EQFreq=10000, EQGain=5, Level=-7),
              S(OscWave="Saw", OscFreq=6400, OscDcy=260, NFilMod="HP", NFilFrq=5400, NFilQ=2.8, NEnvDcy=720, Mix=92, EQFreq=10000, EQGain=7, Level=-7)),
 ("shaker",   S(OscWave="Saw", OscFreq=6200, OscDcy=20, NFilMod="BP", NFilFrq=5600, NFilQ=0.9, NEnvMod="lin", NEnvAtk=30, NEnvDcy=120, Mix=96, Level=-7, NVel=80),
              S(OscWave="Saw", OscFreq=6200, OscDcy=20, NFilMod="BP", NFilFrq=4800, NFilQ=1.4, NEnvMod="lin", NEnvAtk=55, NEnvDcy=260, Mix=96, Level=-7, NVel=80)),
 ("lo tom",  S(OscFreq=96, OscDcy=380, ModAmt=22, ModRate=60, Mix=4, NFilFrq=1200, NEnvDcy=40, Level=-1, ModVel=30),
              S(OscFreq=84, OscDcy=650, ModAmt=30, ModRate=48, Mix=6, NFilFrq=1200, NEnvDcy=60, Level=-1, DistAmt=20, ModVel=30)),
 ("rim",  S(OscWave="Triangle", OscFreq=1750, OscDcy=22, ModAmt=-8, ModRate=24, NFilMod="BP", NFilFrq=4100, NFilQ=2, NEnvDcy=24, Mix=40, DistAmt=24, Level=-4),
              S(OscWave="Triangle", OscFreq=1300, OscDcy=46, ModAmt=-14, ModRate=24, NFilMod="BP", NFilFrq=3200, NFilQ=2.8, NEnvDcy=60, Mix=50, DistAmt=50, Level=-5)),
 ("crash",    S(OscWave="Saw", OscFreq=5200, OscDcy=260, ModMode="Noise", ModRate=2400, ModAmt=14, NFilMod="HP", NFilFrq=4700, NFilQ=1.5, NEnvDcy=1600, Mix=80, EQFreq=8000, EQGain=6, Level=-8),
              S(OscWave="Saw", OscFreq=4400, OscDcy=520, ModMode="Noise", ModRate=3600, ModAmt=22, NFilMod="HP", NFilFrq=3600, NFilQ=2.2, NEnvDcy=3600, Mix=86, EQFreq=8000, EQGain=8, Level=-9)),
 ("clap",     S(OscFreq=900, OscDcy=18, NFilMod="BP", NFilFrq=1500, NFilQ=2.2, NEnvMod="mod", NEnvDcy=240, Mix=94, DistAmt=18, Level=-3),
              S(OscFreq=900, OscDcy=18, NFilMod="BP", NFilFrq=1200, NFilQ=3.2, NEnvMod="mod", NEnvDcy=520, Mix=96, DistAmt=40, Level=-4)),
 ("bass",     S(OscWave="Saw", OscFreq=55, OscDcy=520, ModAmt=2, ModRate=40, NFilMod="LP", NFilFrq=240, NFilQ=2.4, NEnvDcy=300, Mix=22, DistAmt=36, Level=-3),
              S(OscWave="Saw", OscFreq=41, OscDcy=900, ModAmt=2, ModRate=40, NFilMod="LP", NFilFrq=420, NFilQ=3.8, NEnvDcy=500, Mix=26, DistAmt=62, Level=-4)),
 ("fm snr", S(OscFreq=210, OscDcy=130, ModMode="Sine", ModRate=300, ModAmt=22, NFilMod="HP", NFilFrq=3000, NFilQ=1.2, NEnvDcy=150, Mix=44, DistAmt=18, Level=-3),
              S(OscFreq=260, OscDcy=210, ModMode="Sine", ModRate=520, ModAmt=36, NFilMod="HP", NFilFrq=2400, NFilQ=1.8, NEnvDcy=260, Mix=52, DistAmt=40, Level=-4)),
 ("zap",      S(OscWave="Saw", OscFreq=2400, OscDcy=210, ModAmt=-70, ModRate=70, DistAmt=22, Mix=10, Level=-8),
              S(OscWave="Saw", OscFreq=1500, OscDcy=420, ModAmt=-50, ModRate=30, DistAmt=60, Mix=24, Level=-9)),
 ("hi tom",   S(OscFreq=190, OscDcy=260, ModAmt=18, ModRate=70, Mix=3, NFilFrq=2400, NEnvDcy=24, Level=-2, ModVel=30),
              S(OscFreq=240, OscDcy=420, ModAmt=26, ModRate=58, Mix=5, NFilFrq=2400, NEnvDcy=40, Level=-2, DistAmt=18, ModVel=30)),
]

def text(d):
    return (f"OscWave: {d['OscWave']}\nOscFreq: {d['OscFreq']} Hz\nOscAtk: {d['OscAtk']} ms\nOscDcy: {d['OscDcy']} ms\n"
            f"ModMode: {'Noise FM' if d['ModMode']=='Noise' else d['ModMode']}\nModRate: {d['ModRate']} {'ms' if d['ModMode']=='Decay' else 'Hz'}\nModAmt: {d['ModAmt']} st\n"
            f"NFilMod: {d['NFilMod']}\nNFilFrq: {d['NFilFrq']} Hz\nNFilQ: {d['NFilQ']}\nNEnvMod: {d['NEnvMod']}\nNEnvAtk: {d['NEnvAtk']} ms\nNEnvDcy: {d['NEnvDcy']} ms\n"
            f"Mix: {100-d['Mix']}% osc, {d['Mix']}% noise\nDistAmt: {d['DistAmt']}%\nEQFreq: {d['EQFreq']} Hz\nEQGain: {d['EQGain']} dB\nLevel: {d['Level']} dB\n"
            f"OscVel: {d['OscVel']}%\nNVel: {d['NVel']}%\nModVel: {d['ModVel']}%\n")

out = Path(sys.argv[1] if len(sys.argv) > 1 else "lab1"); out.mkdir(exist_ok=True)
for i, (name, a, b) in enumerate(KIT):
    for tag, d in (("A", a), ("B", b)):
        (out / f"{i:02d}-{name.replace(' ', '_')}-{tag}.mtdrum").write_text(text(d))
print(len(KIT), "sounds ->", out)
