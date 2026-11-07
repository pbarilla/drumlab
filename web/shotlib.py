#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Runs the host simulator (the real UI code) on a script and gives back named screens. Used by make_images.py.
A script is built with the panel language of tests/lab_gen.py (tap, press, key, turn, shot); shot(name) names the frame."""
import os, subprocess, importlib.util, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
spec = importlib.util.spec_from_file_location("lg", os.path.join(ROOT, "tests", "lab_gen.py"))
lg = importlib.util.module_from_spec(spec); spec.loader.exec_module(lg)
SELECT, ALGO, K1, K2, K3, K4, PRESETS = lg.SELECT, lg.ALGO, lg.K1, lg.K2, lg.K3, lg.K4, lg.PRESETS

class Run(lg.S):
    def __init__(s):
        super().__init__(); s.names = {}
    def shot(s, name=None, wait=0):
        s.t += wait
        if name: s.names[s.t] = name
        super().shot()
    def hold(s, b, ms=750):                 # a long press
        s.press(b); s.t += ms; s.release(b)
    def chord(s, a, b):                     # GLO + HOME
        s.press(a); s.press(b); s.t += 60; s.release(b); s.release(a)
    def turns(s, enc, d, n=1, gap=40):
        for _ in range(n): s.turn(enc, d); s.t += gap
    def keys(s, *ks, hold=50):
        for k in ks: s.key(k, hold)
    def chordkeys(s, *ks, hold=120):        # several keys at once
        m = 0
        for k in ks: m |= 1 << lg.W[k]
        s.L.append(f"{s.t} keys {m:#x}"); s.t += hold; s.L.append(f"{s.t} keys 0"); s.t += 50

def run(r, outdir, lab, flash=None, tag="r"):
    """r: a Run. Returns {name: path to ppm}."""
    os.makedirs(outdir, exist_ok=True)
    txt = os.path.join(outdir, tag + ".txt")
    r.save(txt)
    env = dict(os.environ)
    env["HOST_FLASH"] = flash or os.path.join(outdir, tag + ".flash")
    if flash is None and os.path.exists(env["HOST_FLASH"]): os.remove(env["HOST_FLASH"])
    subprocess.run([lab, os.path.join(outdir, tag), txt], env=env, check=True, stdout=subprocess.DEVNULL)
    out = {}
    for t, n in r.names.items():
        p = os.path.join(outdir, f"{tag}-{t}.ppm")
        if not os.path.exists(p): raise SystemExit(f"missing frame {n} at {t}")
        out[n] = p
    return out
