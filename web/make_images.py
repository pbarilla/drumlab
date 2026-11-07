#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Every picture on the site, from the host simulator (the real UI code): the panel pictures of the tutorials and the guide
(site/manual/img/pn-*.png) and the plain screens (site/img/*.png).
   cc -O2 -DCHIPBOX_HOST -DAPP_VER='"dev"' -Ifirmware/src -Ifirmware/gen -o $SCRATCH/lab firmware/src/lab.c
   python3 web/make_images.py $SCRATCH/lab $SCRATCH/shots
Each run starts on a fresh flash and does what the page says, in order. Needs Pillow."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from shotlib import *
from panel import panel
from PIL import Image

ROOT = os.path.dirname(HERE)
PN = os.path.join(ROOT, "site", "manual", "img")
IMG = os.path.join(ROOT, "site", "img")
LAB = sys.argv[1] if len(sys.argv) > 1 else "build/host/lab"
TMP = sys.argv[2] if len(sys.argv) > 2 else "build/host/shots"
W = lg.W
K = lambda *steps: [W[i - 1] for i in steps]          # white keys by their number 1-16 -> the panel's key numbers

JOBS = []                                              # (name, rings, screen, keys)
def job(name, rings, screen, keys=()): JOBS.append((name, list(rings), screen, list(keys)))
SCREENS = {}

# ---------------------------------------------------------------- the tutorials, in the order of the pages
def tutorials():
    r = Run(); r.wait(500)
    # Your first beat
    r.hold("HOME"); r.wait(400); r.shot("picker")
    r.turns(SELECT, 1, 4); r.wait(400); r.shot("new")
    r.tap("HOME"); r.wait(500); r.shot("open")
    r.tap("REC"); r.wait(300); r.shot("write")
    for s in (1, 5, 9, 13): r.key(s - 1)
    r.wait(300); r.shot("kick")
    r.turn(PRESETS, 1); r.wait(300); r.shot("presets")
    for s in (5, 13): r.key(s - 1)
    r.wait(300); r.shot("snare")
    r.turn(PRESETS, 1); r.wait(200)
    for s in (3, 7, 11, 15): r.key(s - 1)
    r.wait(300); r.shot("hat")
    r.tap("REC"); r.wait(100); r.tap("PLAY"); r.wait(1450); r.shot("play")
    r.turns(K1, 1, 8); r.wait(300); r.shot("tempo")
    r.tap("ENV"); r.wait(200)
    for s in (5, 13): r.key(s - 1)
    r.wait(200); r.shot("accent")
    r.tap("ENV"); r.wait(100)
    r.tap("PLAY"); r.wait(300); r.tap("SAVE"); r.wait(120); r.shot("save")
    # Make a kit
    r.wait(700); r.tap("HOME"); r.wait(800); r.shot("snd")
    r.tap("OCTUP"); r.wait(900); r.shot("kit1")
    r.tap("PLAY"); r.wait(500); r.shot("kit2")
    r.turn(SELECT, 1); r.wait(300); r.turn(SELECT, 1); r.wait(500); r.shot("kitlist")
    r.turn(SELECT, -1); r.wait(300); r.turn(SELECT, -1); r.wait(300); r.turn(SELECT, -1); r.wait(500); r.shot("kit3")
    r.tap("OCTUP"); r.wait(600); r.shot("kept")
    r.keys(0); r.wait(300); r.turns(K2, 1, 6); r.wait(500); r.shot("freq")
    r.turn(ALGO, 1); r.wait(500); r.turn(ALGO, 1); r.wait(600); r.shot("noise")
    r.tap("SEL"); r.wait(600); r.shot("patchb")
    r.tap("PLAY"); r.wait(500); r.shot("loop")
    r.tap("PLAY"); r.wait(200)
    r.tap("HOME"); r.wait(300); r.tap("HOME"); r.wait(500); r.tap("PLAY"); r.wait(900); r.shot("again")
    r.tap("PLAY"); r.wait(300)
    # Send to a PO-32
    r.tap("HOME"); r.wait(300); r.tap("HOME"); r.wait(800); r.shot("send")
    r.tap("PLAY"); r.wait(500); r.shot("ask")
    r.tap("OCTUP"); r.wait(2500); r.shot("sending")
    r.tap("PLAY"); r.wait(600)
    # Sync with Smileyface
    r.chord("GLO", "HOME"); r.wait(500); r.shot("menu")
    r.tap("OCTUP"); r.wait(400); r.shot("clock")
    r.tap("HOME"); r.wait(500)
    r.tap("HOME"); r.wait(300); r.tap("PLAY"); r.wait(900); r.shot("run")
    d = run(r, TMP, LAB, tag="tut")
    SCREENS.update({"t-" + k: v for k, v in d.items()})
    job("t-picker", ["HOME"], "t-picker"); job("t-new", ["SELECT"], "t-new"); job("t-open", ["HOME"], "t-open")
    job("t-write", ["REC"], "t-write"); job("t-kick", [], "t-kick", K(1, 5, 9, 13)); job("t-presets", ["PRESETS"], "t-presets")
    job("t-snare", [], "t-snare", K(5, 13)); job("t-hat", [], "t-hat", K(3, 7, 11, 15))
    job("t-play", ["PLAY"], "t-play"); job("t-tempo", ["KNOB1"], "t-tempo");
    job("t-env", ["ENV"], "t-accent"); job("t-accent", [], "t-accent", K(5, 13)); job("t-save", ["SAVE"], "t-save")
    job("k-home", ["HOME"], "t-snd"); job("k-octup", ["OCT+"], "t-kit1"); job("k-roll", ["PLAY"], "t-kit2"); job("k-select", ["SELECT", "PRESETS"], "t-kitlist")
    job("k-keep", ["OCT+"], "t-kept"); job("k-keys", [], "t-kept", K(1, 2, 3, 4)); job("k-freq", ["KNOB2"], "t-freq"); job("k-algo", ["ALGORITHM"], "t-noise")
    job("k-sel", ["SEL"], "t-patchb"); job("k-loop", ["PLAY"], "t-loop"); job("k-again", ["HOME", "PLAY"], "t-again")
    job("s-home", ["HOME"], "t-send"); job("s-play", ["PLAY"], "t-ask"); job("s-yes", ["OCT+"], "t-sending")
    job("y-menu", ["GLO", "HOME"], "t-menu"); job("y-clock", ["OCT+"], "t-clock"); job("y-play", ["PLAY"], "t-run")


# ---------------------------------------------------------------- the lessons
def lessons():
    r = Run(); r.wait(500)
    r.chord("GLO", "HOME"); r.wait(500)
    r.turns(SELECT, 1, 2); r.wait(500); r.shot("row")
    r.tap("OCTUP"); r.wait(900); r.shot("start")
    for s in (1, 5, 9): r.key(s - 1)
    r.wait(500); r.shot("half")
    d = run(r, TMP, LAB, tag="les")
    SCREENS.update({"l-" + k: v for k, v in d.items()})
    job("l-row", ["SELECT"], "l-row"); job("l-start", ["OCT+"], "l-start"); job("l-half", [], "l-half", K(1, 5, 9))


# ---------------------------------------------------------------- the guide (on the demos a fresh device has)
def guide():
    r = Run(); r.wait(600)
    r.shot("home")
    r.tap("PLAY"); r.wait(1700); r.shot("playing"); r.tap("PLAY"); r.wait(200)
    r.turn(PRESETS, 4); r.wait(300); r.keys(0); r.wait(400); r.shot("presets")
    r.turn(SELECT, 1); r.wait(400); r.shot("pattern")
    r.turn(SELECT, -1); r.wait(300)
    r.tap("REC"); r.wait(300); r.shot("write")
    r.key(1); r.wait(200); r.shot("steps")
    r.tap("REC"); r.wait(100)
    r.tap("ENV"); r.wait(300); r.shot("accent"); r.tap("ENV")
    r.tap("ARP"); r.wait(300); r.keys(1); r.wait(400); r.shot("mute")
    r.tap("ARP"); r.wait(300); r.shot("solo"); r.tap("ARP"); r.wait(200)
    r.tap("SEL"); r.wait(300); r.shot("chain"); r.tap("SEL"); r.wait(200)
    r.turns(K1, 1, 6); r.wait(300); r.shot("tempo")
    r.turns(K2, 1, 6); r.wait(300); r.shot("swing")
    r.turns(K3, 1, 8); r.turns(K4, 1, 8); r.wait(300); r.shot("pm")
    r.press("FX"); r.wait(300); r.shot("fx"); r.release("FX"); r.wait(300)
    r.tap("SAVE"); r.wait(120); r.shot("save")
    r.tap("HOME"); r.wait(800); r.shot("snd")
    for i, n in enumerate(("mod", "noise", "env", "out", "sound")):
        r.turn(ALGO, 1); r.wait(550); r.shot("p-" + n)
    r.turn(ALGO, 1); r.wait(500)
    r.tap("SEL"); r.wait(600); r.shot("ab")
    r.tap("SEL"); r.wait(200); r.tap("PLAY"); r.wait(400); r.shot("loop"); r.tap("PLAY")
    r.tap("OCTUP"); r.wait(900); r.shot("kits")
    r.turn(SELECT, 1); r.wait(300); r.turn(SELECT, 1); r.wait(500); r.shot("kitlist")
    r.tap("OCTDN"); r.wait(500)
    r.tap("HOME"); r.wait(800); r.shot("send")
    r.tap("PLAY"); r.wait(500); r.shot("ask"); r.tap("OCTDN"); r.wait(300)
    r.hold("HOME"); r.wait(500); r.shot("picker")
    r.press("EDIT"); r.t += 800; r.shot("delete"); r.release("EDIT"); r.tap("OCTDN"); r.wait(200)
    r.tap("HOME"); r.wait(300)
    r.tap("GLO"); r.wait(400); r.shot("help"); r.tap("GLO"); r.wait(200)
    r.chord("GLO", "HOME"); r.wait(500); r.shot("menu")
    r.tap("OCTUP"); r.wait(300); r.shot("clock")
    r.turn(SELECT, 1); r.wait(300); r.tap("OCTUP"); r.wait(300); r.shot("chan")
    r.turn(SELECT, 1); r.wait(300); r.shot("lessons")
    d = run(r, TMP, LAB, tag="gd")
    SCREENS.update({"g-" + k: v for k, v in d.items()})
    job("g-home", ["HOME"], "g-home"); job("g-presets", ["PRESETS"], "g-presets"); job("g-pattern", ["SELECT"], "g-pattern")
    job("g-write", ["REC"], "g-write"); job("g-steps", [], "g-steps", K(2)); job("g-accent", ["ENV"], "g-accent"); job("g-mute", ["ARP"], "g-mute")
    job("g-solo", ["ARP"], "g-solo"); job("g-chain", ["SEL"], "g-chain"); job("g-tempo", ["KNOB1"], "g-tempo"); job("g-swing", ["KNOB2"], "g-swing")
    job("g-pm", ["KNOB3", "KNOB4"], "g-pm"); job("g-fx", ["FX"], "g-fx"); job("g-save", ["SAVE"], "g-save")
    job("g-snd", ["HOME"], "g-snd")
    for n in ("mod", "noise", "env", "out", "sound"): job("g-p-" + n, ["ALGORITHM"], "g-p-" + n)
    job("g-knobs", ["KNOB1", "KNOB2", "KNOB3", "KNOB4"], "g-snd")
    job("g-ab", ["SEL"], "g-ab"); job("g-loop", ["PLAY"], "g-loop"); job("g-kits", ["OCT+"], "g-kits"); job("g-kitlist", ["SELECT"], "g-kitlist")
    job("g-send", ["HOME"], "g-send"); job("g-ask", ["PLAY"], "g-ask"); job("g-picker", ["HOME"], "g-picker"); job("g-delete", ["EDIT"], "g-delete")
    job("g-help", ["GLO"], "g-help"); job("g-menu", ["GLO", "HOME"], "g-menu"); job("g-clock", ["OCT+", "OCT-"], "g-clock"); job("g-chan", ["OCT+", "OCT-"], "g-chan")
    job("g-lessons", ["SELECT"], "g-lessons")
    # the pictures of the front page and the README: plain screens, twice the size
    for n in ("playing", "snd", "kits", "send", "picker", "menu"): job("@" + n, [], "g-" + n)


def main():
    os.makedirs(PN, exist_ok=True); os.makedirs(IMG, exist_ok=True)
    tutorials(); lessons(); guide()
    n = 0
    for name, rings, scr, keys in JOBS:
        im = Image.open(SCREENS[scr])
        if name.startswith("@"):
            im.resize((480, 480), Image.NEAREST).save(os.path.join(IMG, name[1:] + ".png"), optimize=True)
        else:
            panel(rings, im, keys, crop=not keys).save(os.path.join(PN, "pn-" + name + ".png"), optimize=True)
        n += 1
    print("wrote", n, "pictures")


if __name__ == "__main__":
    main()
