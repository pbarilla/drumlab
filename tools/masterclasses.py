#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The lessons that are made from data: the masterclasses (long lessons that teach WHY a drum sounds as it does: pages to read alternate with things to do) and the newer
# guided lessons, with the patches they start from. This file is the source. It writes firmware/src/lessons_mc_<id>.h (one masterclass each), lessons_guided.h (guided
# lessons, placed at the end of GLESSON[]) and lessons_mc_patches.h (the patch table); tests/gl_test.py reads the same lists to do each lesson.   python3 tools/masterclasses.py
#
# PAGES   R(title, body, patch): title 20 characters at most; body wrapped to 25 characters a line, 10 lines a page (a long body becomes several pages, split at a sentence).
#         patch: the number of a patch (below) the chapter starts from, set when the page shows; 0 keeps the sound as it is.
# STEPS   D(text, value, op, number, aux, action). There is no pill: the text names its control ('Turn KNOB 1 for FREQ'), 26 characters at most, verb first. A knob step is a text that starts
#         'Turn KNOB' with op CHG: it needs 4+ clicks of a knob (counted over all knobs).
#         value: a GV_ name without the prefix (see lablesson.c): VIEW WR HITS ACC ACCM SEL BPM SWING K3 K4 CURPAT CHAINM CHAIN MUTEM RUN FX FXM KEYS UNDOS PG SIDE KP KITS ASK KITON
#           PA / PB / PCUR (aux = a parameter, 0..20: patch A / patch B / the side on show; raw 0..65535) PQ (the same, 12 bits: UP / DOWN n is about n / 70 clicks) PSEG (aux = param: 0 1 2 = which
#           third a lamp-like parameter is in) LOCKP LOCKM (steps with their own pitch / morph) FXPAD (aux = pad 0..15, held) FXREC (steps with an effect recorded) LOOP FINET KITSEL.
#           PARAMETERS (aux): OscWave 0 OscFreq 1 OscAtk 2 OscDcy 3 ModMode 4 ModRate 5 ModAmt 6 NFilMod 7 NFilFrq 8 NFilQ 9 NEnvMod 10 NEnvAtk 11 NEnvDcy 12 Mix 13 DistAmt 14 EQFreq 15 EQGain 16 Level 17
#           OscVel 18 NVel 19 ModVel 20 (PN['OscFreq'] ...).
#         op: EQ UP DOWN CHG (UP / DOWN n: n more / less than when the step began; n is 8 bits).
#         action: what the host test does: ("tap", 'PLAY') ("key", i) ("turn", enc, clicks) exact clicks (K1 .. K4, SELECT, ALGO, PRESETS) ("alg", page) ALGORITHM to a page (the generator counts)
#           ("hold", 'EDIT', ms) ("py", "code using s") with s the script (s.tap s.press s.release s.key s.keydown s.keyup s.turn s.wait) and the encoders as constants. A page needs none (OCT+ is pressed).
# PATCHES patch(mc, id, sel=0, a=vec, b=None, side=0, pg=0, view='LAB', pitch=128, morph=0, hits=None, accents=(), bpm=0): vec = the 21 parameters (vec(OscFreq=..., ...) over the defaults, or a
#         full list); b the patch B (None: a copy of A); hits = {step: [sounds 0..15]} for pattern 1 (None: keep the pattern); accents = steps; bpm 0 keeps it. mc 99 = a guided lesson's start.
import os, re, textwrap

PNAMES = ["OscWave", "OscFreq", "OscAtk", "OscDcy", "ModMode", "ModRate", "ModAmt", "NFilMod", "NFilFrq", "NFilQ", "NEnvMod", "NEnvAtk", "NEnvDcy", "Mix", "DistAmt", "EQFreq", "EQGain", "Level", "OscVel", "NVel", "ModVel"]
PN = {n: i for i, n in enumerate(PNAMES)}
PDEF = [10923, 10423, 0, 33172, 10923, 60291, 32768, 10923, 30539, 11077, 10923, 0, 3192, 0, 0, 37115, 32768, 52694, 0, 0, 0]       # a plain sine kick (the research's P1)
def vec(base=None, **kw):
    v = list(base or PDEF)
    for k, x in kw.items(): assert k in PN, k; v[PN[k]] = int(x)
    assert len(v) == 21 and all(0 <= x <= 65535 for x in v)
    return v

class R:
    def __init__(s, title, body, patch=0): s.title, s.body, s.patch = title, " ".join(body.split()), patch
def D(text, v, op, n, a, act): return (text, v, op, n, a, act)
def KEY(text="Press a key to hear it", k=0): return D(text, "KEYS", "UP", 1, 0, ("key", k))
def PLAY(text="Press PLAY to hear it"): return D(text, "RUN", "EQ", 1, 0, ("tap", "PLAY"))
def STOP(text="Press PLAY again to stop"): return D(text, "RUN", "EQ", 0, 0, ("tap", "PLAY"))
def KN(n, text, v, op, num, a, clicks): return D(text, v, op, num, a, ("turn", n, clicks))
def ALG(page, text): return D(text, "PG", "EQ", page, 0, ("alg", page))

MC = []                                          # (id, name, items)
GUIDED = []                                      # (name, start patch or 0, items)
INTRO = []                                       # the first lessons (placed before everything else): the same
INTROS = {}                                      # a lesson name -> the page that starts it (the lessons written by hand in lablesson.c)
PATCHES = []
def masterclass(mid, name, *items): MC.append((mid, name, list(items)))
def guided(name, start, *items): GUIDED.append((name, start, list(items)))
def intro(name, start, *items): INTRO.append((name, start, list(items)))
def page(name, title, body): INTROS[name] = R(title, body)
def patch(mc, pid, sel=0, a=None, b=None, side=0, pg=0, view="LAB", pitch=128, morph=0, hits=None, accents=(), bpm=0): PATCHES.append((mc, pid, sel, a or PDEF, b, side, pg, view, pitch, morph, hits, tuple(accents), bpm))

# ===== the content goes below this line =====

# ---- helpers
def ALGR(page, text, d): return D(text, "PG", "EQ", page, 0, ("py", "s.turn(ALGO, %d)" % d))      # ALGORITHM relative to where the page was
def PCHG(n, text, a, clicks): return KN(n, text, "PCUR", "CHG", 0, a, clicks)
def SEG(n, text, a, third, clicks): return KN(n, text, "PSEG", "EQ", third, a, clicks)
def HOME_LAB(): return D("Tap HOME until LAB shows", "VIEW", "EQ", 1, 0, ("tap", "HOME"))
def HOME_PTN(): return D("Tap HOME until PTN shows", "VIEW", "EQ", 0, 0, ("py", "s.tap('HOME'); W(s, 300); s.tap('HOME')"))
def REC_ON(): return D("Tap REC to turn WRITE on", "WR", "EQ", 1, 0, ("tap", "REC"))
def REC_OFF(): return D("Tap REC to turn WRITE off", "WR", "EQ", 0, 0, ("tap", "REC"))
def KEYN(k, text=None): return KEY(text or "Press key %d to hear it" % (k + 1), k)
BEAT = {0: [0], 4: [0, 1], 8: [0], 12: [0, 1], 2: [2], 6: [2], 10: [2], 14: [2]}
K1A = [0, 9065, 0, 33172, 0, 60593, 54951, 0, 30538, 11077, 0, 0, 3192, 3932, 5243, 37114, 32768, 55893, 19661, 32768, 13107]    # LAB-1 kick A / B
K1B = [0, 7902, 0, 39155, 0, 57204, 56884, 0, 33730, 11077, 0, 0, 6576, 5243, 22282, 37114, 32768, 54826, 19661, 32768, 13107]
SN1 = [32768, 21105, 0, 21358, 0, 62967, 45281, 65535, 44594, 12507, 0, 0, 27934, 40632, 9175, 37114, 32768, 53760, 32768, 22937, 0]   # LAB-1 snare A

P1 = [10923, 10423, 0, 33172, 10923, 60291, 32768, 10923, 30539, 11077, 10923, 0, 3192, 0, 0, 37115, 32768, 52694, 0, 0, 0]
P2 = [32768, 21106, 0, 21358, 10923, 62982, 45281, 54613, 44595, 12507, 10923, 0, 27934, 0, 0, 37115, 32768, 52694, 0, 0, 0]
P3 = [10923, 56603, 0, 5576, 10923, 57234, 32768, 10923, 56104, 11077, 10923, 0, 13615, 65535, 0, 37115, 32768, 48444, 0, 0, 0]
M2A = [10923, 10423, 0, 23575, 10923, 61945, 52833, 10923, 34998, 11077, 10923, 0, 1730, 6620, 0, 37115, 32768, 51630, 0, 0, 0]
M2B = [10923, 7694, 0, 41573, 10923, 46602, 47724, 10923, 34998, 11077, 10923, 0, 1730, 6620, 0, 37115, 32768, 51630, 0, 0, 0]
M2WA = [10923, 19117, 0, 32268, 10923, 57234, 32768, 10923, 37115, 11077, 10923, 0, 0, 0, 0, 37115, 32768, 50567, 0, 0, 0]
M2WB = list(M2WA); M2WB[0] = 54613; M2WB[7] = 54613
F1 = [10923, 30911, 0, 46179, 10923, 52958, 32768, 10923, 37115, 11077, 10923, 0, 0, 0, 0, 37115, 32768, 48444, 0, 0, 0]
F2 = [10923, 25693, 0, 41573, 10923, 39719, 32768, 10923, 37115, 11077, 10923, 0, 0, 0, 0, 37115, 32768, 48444, 0, 0, 0]
F3 = [10923, 34998, 0, 0, 10923, 57234, 32768, 10923, 34998, 19860, 10923, 0, 34997, 65535, 0, 37115, 32768, 36400, 0, 0, 0]       # level lowered: a high Q gets loud
F4 = [10923, 34998, 0, 0, 10923, 57234, 32768, 32768, 38845, 19860, 10923, 0, 46338, 65535, 0, 37115, 32768, 48444, 0, 0, 0]
F5 = [10923, 15270, 0, 38844, 10923, 57234, 32768, 10923, 37115, 11077, 10923, 0, 0, 0, 0, 37115, 32768, 48444, 0, 0, 0]
F6 = [10923, 50267, 0, 43690, 10923, 23169, 52958, 10923, 37115, 11077, 10923, 0, 0, 0, 0, 37115, 32768, 48444, 0, 0, 0]
def v(base, **kw): return vec(base, **kw)

# ================= MASTERCLASS 1: Anatomy =================
patch(1, 1, sel=0, a=P1)
patch(1, 2, sel=0, a=vec(P1, OscWave=54613))                                              # a saw: it clicks
patch(1, 3, sel=0, a=vec(P1, NFilMod=54613, NFilFrq=45000))                               # a sine with a bright, short noise ready
patch(1, 4, sel=1, a=v(P2, Mix=0), pg=0)                                                  # the snare, tone only
patch(1, 5, sel=1, a=v(P2, Mix=32768), pg=3)                                              # tone and noise, ENV page
patch(1, 6, sel=1, a=v(P2, Mix=32768), pg=3)
patch(1, 7, sel=9, a=v(F3, NFilQ=6620), pg=2)                                             # ringing noise, Q 10 on screen
patch(1, 8, sel=2, a=P3, pg=2)                                                            # the hat, LP
patch(1, 9, sel=1, a=v(P2, Mix=32768), pg=0)                                              # snare for the EQ
patch(1, 10, sel=0, a=K1A)                                                                # the LAB-1 kick
patch(1, 11, sel=1, a=v(P2, Mix=32768), pg=2)                                             # snare, NOISE page

masterclass(1, "Masterclass: Anatomy",
  R("A drum is a fade", "A drum hit is a tone that fades. Three things shape it: how long it rings, how high it is, what it is made of. The panel holds a plain sine at 60 Hz.", patch=1),
  KEYN(0),
  R("DECAY: how long", "DECAY is the time to fall 60 dB, close to silence. 10 ms at the bottom, 10 s at the top. 20 is a 40 ms tick, 40 is 160 ms, 60 is 660 ms, 80 is 2.7 s.", patch=1),
  PCHG(4, "Turn KNOB 4 for DECAY", 3, 8),
  KEYN(0),
  R("FREQ: how high", "FREQ runs 20 Hz to 20 kHz on a curve. Six clicks are one octave. A kick lives at 40 to 60 Hz, a snare or tom at 150 to 250, a hat above 5 kHz.", patch=1),
  PCHG(2, "Turn KNOB 2 for FREQ", 1, 8),
  KEYN(0),
  R("WAVE: the colour", "A sine is one pitch and nothing else. A triangle adds odd harmonics, the 3rd 19 dB down: hollow and soft. A saw adds every harmonic, the 2nd only 6 dB down: bright and buzzy.", patch=1),
  SEG(1, "Turn KNOB 1 for TRI", 0, 1, 1),
  KEYN(0),
  SEG(1, "Turn KNOB 1 for SAW", 0, 2, 1),
  KEYN(0),
  R("The click", "A sine starts at zero. A triangle starts at its peak and a saw at its bottom, so the first sample jumps: a click. ATTACK fades the start in. At 10 it takes 7 ms.", patch=2),
  PCHG(3, "Turn KNOB 3 for ATTACK", 2, 6),
  KEYN(0),
  R("Why a kick falls", "A real kick head loosens as it rings, so its pitch falls. MOD copies that. AMOUNT is how far above FREQ the pitch starts, in semitones. RATE is how fast it falls back.", patch=1),
  ALG(1, "Turn ALGORITHM to MOD page"),
  PCHG(3, "Turn KNOB 3 for AMOUNT", 6, 12),
  KEYN(0),
  PCHG(2, "Turn KNOB 2 for RATE", 5, 8),
  KEYN(0),
  R("The numbers", "AMOUNT 50 is zero. 60 is +4 semitones, 70 is +16, 80 is +36, 90 is +64, 99 is +96. Below 50 it starts low and rises. RATE 99 halves the gap in 1 ms, 70 in 15 ms, 50 in 35 ms. RATE 0 never falls."),
  R("The LAB-1 kick", "The kick in LAB-1: a 52 Hz sine, AMOUNT +44 semitones so it starts near 660 Hz, RATE halves the gap every 3.8 ms, DECAY 330 ms. A touch of noise (MIX 6) gives the click.", patch=10),
  KEYN(0),
  R("A touch of noise", "Noise is the second source. MIX blends: 0 is the oscillator alone, 50 both at full, 99 noise only. The noise has its own short DECAY on the ENV page, so a little of it is a click.", patch=3),
  ALG(3, "Turn ALGORITHM to ENV page"),
  PCHG(4, "Turn KNOB 4 for MIX", 13, 16),
  KEYN(0),
  R("Snare: tone + noise", "A snare is a shell and wires. The shell is a tone: here a 185 Hz triangle. The wires are noise. MIX 25 lets in noise 18 dB down. At 50 both are full. Above 50 the tone fades.", patch=4),
  ALG(3, "Turn ALGORITHM to ENV page"),
  PCHG(4, "Turn KNOB 4 for MIX", 13, 30),
  KEYN(1),
  R("Noise has its tail", "The noise fades on its own clock: the tone takes 95 ms, the noise 190 ms. A longer noise DECAY makes a swish. A shorter one makes a tap.", patch=5),
  PCHG(3, "Turn KNOB 3 for DECAY", 12, 16),
  KEYN(1),
  R("Filter: shape noise", "Noise holds every frequency. A filter keeps some. LP keeps what is below FREQ, HP what is above, BP a band around it. LP and HP lose 12 dB per octave beyond FREQ.", patch=6),
  ALGR(2, "Turn ALGORITHM to NOISE", -1),
  SEG(1, "Turn KNOB 1 for LP", 7, 0, -2),
  SEG(1, "Turn KNOB 1 for HP", 7, 2, 2),
  PCHG(2, "Turn KNOB 2 for FREQ", 8, 8),
  KEYN(1),
  R("Q: resonance", "Q sharpens the filter. On the knob, 20 is a Q of 1, 40 a Q of 10, 60 a Q of 100. LP and HP grow a peak: about 1 dB at Q 1, 20 dB at Q 10, 40 dB at Q 100. It gets loud, so mind LEVEL.", patch=7),
  PCHG(3, "Turn KNOB 3 for Q", 9, 16),
  KEYN(9),
  R("Hat: bright noise", "A hat is noise with the lows cut, and a short tail. MIX is all noise. Noise DECAY 20 is 40 ms, a closed hat. 50 is 330 ms, an open one. The panel starts LP: dull.", patch=8),
  SEG(1, "Turn KNOB 1 for HP", 7, 2, 2),
  KEYN(2),
  ALGR(3, "Turn ALGORITHM to ENV page", 1),
  PCHG(3, "Turn KNOB 3 for DECAY", 12, 16),
  KEYN(2, "Press key 3 for open hat"),
  R("DIST: grit", "DIST 0 and 1 are off. Up to 16 it bends the wave a little, adding even and odd harmonics. From 17 it clips both sides: odd harmonics only. At 99 a sine is nearly a square, 4.6 dB quieter.", patch=1),
  ALG(4, "Turn ALGORITHM to OUT page"),
  PCHG(1, "Turn KNOB 1 for DIST", 14, 30),
  KEYN(0),
  R("EQ: one bell", "EQ HZ picks a centre, EQ DB lifts or cuts it by up to 40 dB. The middle is flat. It is one bell, Q 1. A boost also lowers the whole level by half its size: +20 dB nets about +10 at the bell.", patch=9),
  ALG(4, "Turn ALGORITHM to OUT page"),
  PCHG(3, "Turn KNOB 3 for EQ DB", 16, 16),
  PCHG(2, "Turn KNOB 2 for EQ HZ", 15, 12),
  KEYN(1),
  R("LEVEL is dB", "LEVEL is volume in dB. 83 is 0 dB, full scale. 80 is -2 dB, 70 is -8, 60 is -14, 50 is -21, 10 is -53. Above 83 the peaks clip: 99 is +10 dB, squashed flat.", patch=1),
  ALG(4, "Turn ALGORITHM to OUT page"),
  PCHG(4, "Turn KNOB 4 for LEVEL", 17, 8),
  KEYN(0),
  R("Make your own", "Kick: sine 45 to 60 Hz, AMOUNT 80, RATE 85, DECAY 50, MIX 5. Snare: triangle 185 Hz, noise HP 2K, MIX 60. Hat: MIX 99, noise HP 7K, noise DECAY 20."),
)

# ================= MASTERCLASS 2: Morph =================
patch(2, 1, sel=0, a=M2A, b=M2B)
patch(2, 2, sel=0, a=M2A, b=M2B, view="PTN", hits={}, morph=0)
patch(2, 3, sel=0, a=M2A, b=M2B, view="PTN", hits={0: [0], 4: [0], 8: [0], 12: [0]}, bpm=120, morph=0)
patch(2, 4, sel=8, a=M2WA, b=M2WB, view="PTN", hits={}, morph=0)

masterclass(2, "Masterclass: Morph",
  R("Two patches", "Every sound is two patches, A and B, 21 numbers each. In LAB, SEL swaps the one you edit; the A | B lamp shows it. Keys play the side on show.", patch=1),
  KEYN(0, "Press key 1 for patch A"),
  D("Tap SEL for patch B", "SIDE", "EQ", 1, 0, ("tap", "SEL")),
  KEYN(0, "Press key 1 for patch B"),
  R("A tight, B boomy", "A: 60 Hz, rings 120 ms, starts 36 semitones up and falls fast. B: 45 Hz, rings 800 ms, starts 20 up and falls slower. A is a tick, B is a boom. The morph moves between them."),
  R("The morph knob", "MORPH blends all 21 numbers: A plus (B minus A) times the morph. 0% is A, 100% is B. On PTN, KNOB 4 is MORPH. A click is 1.6%."),
  HOME_PTN(),
  D("Turn KNOB 4 to half way", "K4", "UP", 120, 0, ("py", "s.turn(K4, 9)")),
  KEYN(0),
  R("Half way", "Pitch and time are log scales, so half way is the geometric mean: 50% of 60 and 45 Hz is 52 Hz. 50% of 120 and 800 ms is 310 ms. A drum between two drums, not two at once."),
  D("Turn KNOB 4 up to B", "K4", "UP", 120, 0, ("py", "s.turn(K4, 9)")),
  KEYN(0),
  R("One value a hit", "A hit reads the morph when it starts and keeps it. The morph never moves inside a sound. Motion comes from hit to hit."),
  R("Steps remember", "Pads use the knob as it is now. A step remembers the knob from when you wrote it. Turn KNOB 4 later and old steps keep theirs.", patch=2),
  REC_ON(),
  D("Press a key to write a hit", "HITS", "UP", 1, 0, ("key", 0)),
  D("Turn KNOB 4 up", "K4", "UP", 100, 0, ("py", "s.turn(K4, 8)")),
  D("Press another key to write", "LOCKM", "UP", 1, 0, ("key", 4)),
  R("A bar that moves", "Write the same drum on more steps, each at a higher morph, and the bar drifts from tight to boomy. Then press PLAY."),
  D("Raise KNOB 4, press a key", "HITS", "UP", 1, 0, ("py", "s.turn(K4, 4); W(s, 300); s.key(8)")),
  D("Raise again, press a key", "HITS", "UP", 1, 0, ("py", "s.turn(K4, 4); W(s, 300); s.key(12)")),
  PLAY(), STOP(),
  R("Lock a step", "Hold a NEW step while you turn KNOB 3 or 4 and the step keeps that pitch or morph. A step that already has the sound would be switched off, so lock on the press that writes it.", patch=2),
  D("Hold a key, turn KNOB 4", "LOCKM", "UP", 1, 0, ("py", "s.keydown(2); W(s, 200); s.turn(K4, 1); W(s, 200); s.keyup()")),
  D("Hold a key, turn KNOB 3", "LOCKP", "UP", 1, 0, ("py", "s.keydown(6); W(s, 200); s.turn(K3, 1); W(s, 200); s.keyup()")),
  R("Mega morph pad", "FX pad 7 is MORPH. While you hold it every hit plays at B, as if the knob were up. Try it on the beat.", patch=3),
  REC_OFF(),
  PLAY("Press PLAY to start"),
  D("Tap FX for the pads", "FX", "EQ", 1, 0, ("tap", "FX")),
  D("Hold pad 7 for MORPH", "FXPAD", "EQ", 1, 6, ("py", "s.keydown(6); W(s, 1200); s.keyup()")),
  D("Tap FX to leave", "FX", "EQ", 0, 0, ("tap", "FX")),
  R("Live morph", "Hold REC while it plays and turn KNOB 4. The knob is written into each step as the head passes. Let go and the bar replays your move."),
  D("Hold REC, turn KNOB 4", "LOCKM", "UP", 1, 0, ("py", "s.press('REC'); W(s, 300); s.turn(K4, 4); W(s, 1500); s.release('REC')")),
  STOP(),
  R("Switches jump", "WAVE and FILTER are three-way switches, and they blend like numbers. Here A is a sine, B a saw. Sine to 24%, triangle 25% to 74%, saw from 75%. The sound jumps twice.", patch=4),
  KEYN(8, "Press key 9 to hear A"),
  D("Turn KNOB 4 to half way", "K4", "UP", 120, 0, ("py", "s.turn(K4, 9)")),
  KEYN(8),
  D("Turn KNOB 4 up to B", "K4", "UP", 120, 0, ("py", "s.turn(K4, 9)")),
  KEYN(8),
  R("Recipes", "A closed hat in A, an open hat in B, and morph on the off-beats. A noise snare in A, a tone snare in B. A kick that grows longer each bar. Write the steps, then morph."),
)

# ================= MASTERCLASS 3: FM =================
patch(3, 1, sel=3, a=F1)
patch(3, 2, sel=3, a=v(F1, ModRate=19860), pg=0)
patch(3, 3, sel=3, a=v(F1, ModMode=32768, ModAmt=46335, ModRate=19860), pg=1)
patch(3, 4, sel=3, a=v(F1, ModMode=32768, ModAmt=46335), pg=0)
patch(3, 5, sel=3, a=v(F1, ModMode=32768, ModAmt=36400), pg=1)
patch(3, 6, sel=4, a=F2, pg=1)
patch(3, 7, sel=9, a=F3, pg=0)
patch(3, 8, sel=11, a=F4, pg=0)
patch(3, 9, sel=11, a=v(F4, NEnvMod=54613), pg=3)
patch(3, 10, sel=12, a=F5, pg=0)
patch(3, 11, sel=14, a=F6, pg=0)
patch(3, 12, sel=0, a=K1A, view="PTN", hits=BEAT, bpm=120, morph=0)

masterclass(3, "Masterclass: FM",
  R("Pitch can move", "MOD moves the pitch of the oscillator. DECAY: a fall that fades. SINE: an LFO, up and down. NOISE: random. Only the oscillator moves, never the noise. The panel holds a plain bell.", patch=1),
  KEYN(3),
  R("SINE: vibrato", "In SINE mode the pitch swings around FREQ. AMOUNT is how far: 60 is +-2 semitones, 70 is +-8, 80 is +-18, 90 is +-32, 99 is +-48. RATE is the speed: 30 is 2 Hz, already set.", patch=2),
  ALG(1, "Turn ALGORITHM to MOD page"),
  SEG(1, "Turn KNOB 1 for SINE mode", 4, 1, 1),
  PCHG(3, "Turn KNOB 3 for AMOUNT", 6, 12),
  KEYN(3),
  R("Fast is FM", "Speed the LFO past 20 Hz and the wobble turns into new tones, spaced by the RATE. 50 is 19 Hz, 60 is 51, 70 is 133, 80 is 344, 90 is 874, 99 is 2000 Hz.", patch=3),
  D("Turn KNOB 2 up for speed", "PQ", "UP", 200, 5, ("turn", 2, 24)),
  KEYN(3),
  R("The ratio", "FREQ against RATE sets the colour. When FREQ is a whole multiple of RATE, the new tones sit on harmonics and you hear a pitch. Anything else is a bell or a clang. Tune by ear.", patch=4),
  PCHG(2, "Turn KNOB 2 for FREQ", 1, 6),
  KEYN(3),
  R("Depth", "AMOUNT is the depth of the FM. A little: a sine with a shimmer. A lot: a bright, rough clang. Below 50 it is the same depth, upside down.", patch=5),
  PCHG(3, "Turn KNOB 3 for AMOUNT", 6, 24),
  KEYN(3),
  R("NOISE: random pitch", "In NOISE mode the pitch is random. RATE smooths it: slow is a gentle wander, fast is a hiss around FREQ. AMOUNT is how far it strays.", patch=6),
  SEG(1, "Turn KNOB 1 for NOISE mode", 4, 2, 2),
  PCHG(3, "Turn KNOB 3 for AMOUNT", 6, 12),
  KEYN(4),
  R("Noise that rings", "A resonant filter turns noise into a tone. A low-pass at a high Q rings at FREQ: from about 55 on the knob it is a clean ping that fades with the noise DECAY. A drum with no oscillator.", patch=7),
  ALG(2, "Turn ALGORITHM to NOISE"),
  PCHG(3, "Turn KNOB 3 for Q", 9, 16),
  KEYN(9),
  R("Clap flutter", "SHAPE MOD multiplies the noise by a cosine, so it dips to silence again and again. DECAY sets the speed: 65 dips every 6 ms, 70 every 11, 75 every 18, 80 every 31. Below about 50 it buzzes.", patch=8),
  ALG(3, "Turn ALGORITHM to ENV page"),
  SEG(1, "Turn KNOB 1 for MOD shape", 10, 2, 2),
  PCHG(3, "Turn KNOB 3 for DECAY", 12, 8),
  KEYN(11),
  R("Hold, then flutter", "In MOD shape ATTACK is a hold: the noise stays full that long, then the flutter starts. Short hold: a clap. Long hold: a burst, then a stutter.", patch=9),
  PCHG(2, "Turn KNOB 2 for ATTACK", 11, 24),
  KEYN(11),
  R("A sine into a square", "DIST clips the top and bottom. At 99 a sine is nearly a square: odd harmonics only, the 3rd 9 dB down, the 5th 15 dB. A square at 100 Hz is a fat bass.", patch=10),
  ALG(4, "Turn ALGORITHM to OUT page"),
  PCHG(1, "Turn KNOB 1 for DIST", 14, 40),
  KEYN(12),
  R("Past the edge", "The oscillator tops out at 19.8 kHz, but the pitch drop can start much higher. Above 22 kHz the tone folds back down: a wild chirp. This sound starts at 32 kHz.", patch=11),
  ALG(1, "Turn ALGORITHM to MOD page"),
  PCHG(2, "Turn KNOB 2 for RATE", 5, 12),
  KEYN(14),
  R("The FM pad", "FX pad 13 is FM: the whole mix is multiplied by a sine of about 340 Hz, a ring modulator. It adds the sum and difference of every tone. Metal.", patch=12),
  PLAY("Press PLAY to start"),
  D("Tap FX for the pads", "FX", "EQ", 1, 0, ("tap", "FX")),
  D("Hold pad 13 for FM", "FXPAD", "EQ", 1, 12, ("py", "s.keydown(12); W(s, 1500); s.keyup()")),
  D("Tap FX to leave", "FX", "EQ", 0, 0, ("tap", "FX")),
)

# ================= GUIDED LESSONS =================
def beat_patch(pid, **kw):
    kw.setdefault("view", "PTN"); patch(99, pid, sel=0, a=K1A, b=K1B, hits=BEAT, bpm=120, **kw)
beat_patch(1)                                                                                     # the beat
patch(99, 2, sel=0, a=M2A, b=M2B, view="PTN", hits={}, morph=0)                                    # a tight and a boomy kick
patch(99, 3, sel=0, a=K1A, b=K1B, view="PTN", hits={}, morph=0)                                    # the LAB-1 kick, an empty pattern
patch(99, 4, sel=1, a=v(P2, Mix=32768), b=None, view="PTN", hits={}, morph=0)                      # a snare with noise

guided("Lock a step", 0,
  REC_ON(),
  D("Hold a key, turn KNOB 3", "LOCKP", "UP", 1, 0, ("py", "s.keydown(0); W(s, 200); s.turn(K3, 1); W(s, 200); s.keyup()")),
  D("Hold another, turn KNOB 4", "LOCKM", "UP", 1, 0, ("py", "s.keydown(4); W(s, 200); s.turn(K4, 1); W(s, 200); s.keyup()")),
  PLAY("Press PLAY to hear them"),
  STOP())
guided("Morph a beat", 2,
  REC_ON(),
  D("Press a key to write a hit", "HITS", "UP", 1, 0, ("key", 0)),
  D("Turn KNOB 4 up toward B", "K4", "UP", 64, 0, ("py", "s.turn(K4, 6)")),
  D("Press a key for a 2nd hit", "LOCKM", "UP", 1, 0, ("key", 4)),
  PLAY("Press PLAY to hear both"),
  STOP())
guided("FX pads", 1,
  PLAY("Press PLAY to start"),
  D("Tap FX for the pads", "FX", "EQ", 1, 0, ("tap", "FX")),
  D("Hold pad 4 for ECHO", "FXPAD", "EQ", 1, 3, ("py", "s.keydown(3); W(s, 600); s.keyup()")),
  D("Hold pad 1 for HALF", "FXPAD", "EQ", 1, 0, ("py", "s.keydown(0); W(s, 600); s.keyup()")),
  D("Hold pad 8 for BEND", "FXPAD", "EQ", 1, 7, ("py", "s.keydown(7); W(s, 1600); s.keyup()")),
  D("Tap FX to leave", "FX", "EQ", 0, 0, ("tap", "FX")))
guided("Record the FX", 1,
  REC_ON(),
  PLAY("Press PLAY to start"),
  D("Hold FX, press a pad", "FXREC", "UP", 1, 0, ("py", "s.press('FX'); W(s, 400); s.keydown(3); W(s, 700); s.keyup(); s.release('FX')")),
  D("Hold FX alone to wipe it", "FXREC", "DOWN", 1, 0, ("py", "s.press('FX'); W(s, 2400); s.release('FX')")),
  STOP())
guided("Punch in live", 1,
  PLAY("Press PLAY to start"),
  D("Hold REC, press keys", "HITS", "UP", 2, 0, ("py", "s.press('REC'); W(s, 400); s.key(3); W(s, 300); s.key(3); W(s, 300); s.release('REC')")),
  D("Tap LFO to undo", "UNDOS", "UP", 1, 0, ("tap", "LFO")),
  D("Hold REC, turn KNOB 4", "LOCKM", "UP", 1, 0, ("py", "s.turn(PRESETS, -3); W(s, 300); s.press('REC'); W(s, 300); s.turn(K4, 6); W(s, 1200); s.release('REC')")),
  STOP())
guided("Accent size", 1,
  D("Tap ENV for accent mode", "ACCM", "EQ", 1, 0, ("tap", "ENV")),
  D("Press 2 keys to mark steps", "ACC", "UP", 2, 0, ("py", "s.key(0); s.key(8)")),
  HOME_LAB(),
  D("Turn ALGORITHM to SOUND", "PG", "EQ", 5, 0, ("py", "s.turn(ALGO, -1)")),
  PCHG(1, "Turn KNOB 1 for VELOC", 18, 8),
  HOME_PTN(),
  PLAY("Press PLAY to hear accents"))
guided("Loop and fine", 3,
  HOME_LAB(),
  D("Press PLAY to loop sound", "LOOP", "EQ", 1, 0, ("tap", "PLAY")),
  PCHG(2, "Turn KNOB 2 for FREQ", 1, 6),
  D("Hold EDIT, turn KNOB 2", "FINET", "UP", 3, 0, ("py", "s.press('EDIT'); W(s, 200); s.turn(K2, 1); W(s, 200); s.release('EDIT')")),
  D("Press PLAY to stop", "LOOP", "EQ", 0, 0, ("tap", "PLAY")))
guided("Mod modes", 3,
  HOME_LAB(),
  ALG(1, "Turn ALGORITHM to MOD page"),
  SEG(1, "Turn KNOB 1 for SINE mode", 4, 1, 1),
  PCHG(2, "Turn KNOB 2 for RATE", 5, 8),
  SEG(1, "Turn KNOB 1 for NOISE mode", 4, 2, 1),
  KEYN(0))
guided("Noise shapes", 3,
  HOME_LAB(),
  D("Turn PRESETS to sound 12", "SEL", "EQ", 11, 0, ("turn", "PRESETS", -5)),
  ALG(3, "Turn ALGORITHM to ENV page"),
  SEG(1, "Turn KNOB 1 for LIN shape", 10, 1, -1),
  SEG(1, "Turn KNOB 1 for MOD shape", 10, 2, 1),
  PCHG(3, "Turn KNOB 3 for DECAY", 12, 8),
  KEYN(11, "Press key 12 to hear it"))
guided("EQ and level", 4,
  HOME_LAB(),
  ALG(4, "Turn ALGORITHM to OUT page"),
  PCHG(3, "Turn KNOB 3 for EQ DB", 16, 12),
  PCHG(2, "Turn KNOB 2 for EQ HZ", 15, 8),
  KEYN(1, "Press key 2 to hear it"),
  PCHG(4, "Turn KNOB 4 for LEVEL", 17, 8))

# ---- the first lessons: the panel, then a first beat (these come first in the list)
def PADS(*ks): return "s.key(%d)" % ks[0] if len(ks) == 1 else "; ".join("s.key(%d); W(s, 150)" % k for k in ks)
intro("Welcome", 0,
  R("Welcome", "Drumlab is a drum machine. You pick drum sounds, place them on a row of 16 steps, and it plays them back as a loop. This tour shows you the panel. OCT+ turns a page. HOME hides a page and GLO brings it back."),
  R("The 16 keys", "The 16 keys along the bottom are drum sounds. Press one to hear it. Later the same keys become your 16 steps."),
  KEY("Press any key to hear it"),
  D("Press 3 more keys", "KEYS", "UP", 3, 0, ("py", PADS(1, 2, 3))),
  R("Four lanes", "The four stripes on the screen are lanes: kicks, snares, hats and percussion. Keys 1, 5, 9 and 13 are four different kicks. Keys 2, 6, 10 and 14 are snares, and so on."),
  R("PRESETS", "Turn PRESETS to pick the sound you are working on. It is the sound your next written hit will use, and the one LAB shows."),
  D("Turn PRESETS one notch", "SEL", "CHG", 0, 0, ("turn", "PRESETS", 1)),
  R("The four knobs", "KNOB 1 to 4 sit in a row, in the same order as the four dials on the screen: TEMPO, SWING, PITCH and MORPH. MASTER is the volume."),
  KN(1, "Turn KNOB 1 for TEMPO", "BPM", "CHG", 0, 0, 6),
  R("Odd button names", "A few buttons are named oddly. REC is the WRITE button. ENV is ACCENT. ARP is MUTE. LFO is UNDO. Hold EDIT to clear a pattern. SEL is a button, SELECT is the knob that picks patterns."),
  R("OCT+ and HOME", "In a lesson, OCT+ turns the page. Out of a lesson, OCT+ on PTN copies the pattern you are on onto the next one. HOME changes view. GLO opens the manual."),
  R("Three views", "Tap HOME to change view. The bar at the bottom of the screen names them. PTN is where you write beats. LAB is where you shape sounds. SEND copies it all to a PO-32."),
  D("Tap HOME until LAB shows", "VIEW", "EQ", 1, 0, ("tap", "HOME")),
  R("This is LAB", "The sound lab. It shows the sound PRESETS picked. ALGORITHM flips between its pages and KNOB 1 to 4 change what is on them. Twist anything: nothing here can break."),
  HOME_PTN(),
  R("You have the panel", "Keys, PRESETS, four knobs and HOME: that is most of it. Next up is Your First Beat. Lessons with a gold name are masterclasses: the deep end, for when you are curious. To come back to any lesson, press GLO and HOME together and open LESSONS."))
intro("Your first beat", 0,
  R("A beat", "A beat is 16 steps that repeat. A step is a slot in the bar, and a hit on it plays a sound when the loop reaches it. The four stripes on the screen are lanes, each with its own 16 steps."),
  R("WRITE", "Normally the keys play sounds. Tap REC to turn WRITE on and they become the 16 steps. Press a key to put a hit on that step, press it again to take it away. The WRITE lamp at the top of the screen lights up."),
  REC_ON(),
  D("Press keys 1, 5, 9 and 13\nfor a kick on each beat", "HITS", "UP", 4, 0, ("py", PADS(0, 4, 8, 12))),
  R("Hear it", "Four dots on the first stripe: your kick. Press PLAY and a marker runs along the steps. When it reaches a dot, you hear the sound. It loops until you stop it."),
  PLAY("Press PLAY to hear it"),
  R("The snare", "Sound 2 is a snare. A snare on steps 5 and 13 is the backbeat, the 2 and 4 you clap along to."),
  D("Turn PRESETS to sound 2", "SEL", "EQ", 1, 0, ("turn", "PRESETS", 1)),
  D("Press keys 5 and 13\nfor the snare", "HITS", "UP", 2, 0, ("py", PADS(4, 12))),
  R("Hats", "Sound 3 is a hi-hat. Hats between the kicks, on steps 3, 7, 11 and 15, give the beat its drive."),
  D("Turn PRESETS to sound 3", "SEL", "EQ", 2, 0, ("turn", "PRESETS", 1)),
  D("Press keys 3, 7, 11, 15\nfor off-beat hats", "HITS", "UP", 4, 0, ("py", PADS(2, 6, 10, 14))),
  STOP("Press PLAY to stop"),
  R("That is a beat", "Kick on the beat, snare on 2 and 4, hats in between: the backbone of house, techno and pop. Next, shape it with sounds, accents and tempo."))

page("SOUNDS", "Sounds", "Every key can use a different sound. Turn PRESETS and the hits you write next use that sound. Kicks, snares, hats and percussion live in different lanes, so a beat can use all four at once.")
page("ACCENTS", "Accents", "An accent is a louder hit. Tap ENV to turn ACCENT on, then press a step to accent it. Press it again to remove it. Accents give a beat bounce.")
page("TEMPO AND SWING", "Tempo and swing", "KNOB 1 sets the tempo from 60 to 240 beats a minute. KNOB 2 sets swing: it delays every second step so the beat shuffles instead of marching. Try both while it plays.")
page("PITCH AND MORPH", "Pitch and morph", "KNOB 3 moves the sound up or down by up to 12 semitones. KNOB 4 morphs it between its patch A and patch B. Both change the sound PRESETS picked. Press a key to hear it.")
page("PATTERNS", "Patterns", "A project holds 16 patterns. Turn SELECT to pick one. Write a verse in one and a variation in the next. OCT+ copies the pattern you are on onto the next one, so you can start from what you have.")
page("CHAIN PATTERNS", "Chains", "A chain plays patterns one after another, up to 64. Tap SEL for chain mode and press keys to add patterns: key 1 is pattern 1. Tap SEL to leave, then PLAY to hear the whole chain.")
page("MUTE AND SOLO", "Mute and solo", "ARP is the mute button. Tap it for mute mode, where a key picks a lane. Tap it again for solo, and a third time to hear everything. Keys 1, 5, 9 and 13 share a lane.")
page("PUNCH-IN FX", "Punch-in FX", "Effects you play live. Tap FX and the 16 keys become pads: hold one for as long as you want its effect, let go and it is gone. Tap FX again to put the pads away.")
page("UNDO AND CLEAR", "Undo and clear", "Made a mistake? Tap LFO to undo your last change. To start a pattern again, hold EDIT to clear it.")
page("SOUND LAB", "Sound lab", "LAB is where a sound is built: a tone, a burst of noise and some effects. ALGORITHM flips between the pages and KNOB 1 to 4 change the four settings on each. The sound plays when you let go of a knob.")
page("PATCH A AND B", "Patch A and B", "Every sound has two versions, patch A and patch B. In LAB, press SEL to choose which one you edit. Back on PTN, KNOB 4 morphs from A to B, so one sound can change as the beat goes round.")
page("ROLL A KIT", "Kits", "A kit is a set of sounds. In LAB, press OCT+ to open the kits and PLAY to roll the dice for a new one. OCT+ keeps the kit. OCT- puts your old one back.")
page("SOUND ENGINES", "Sound engines", "Each sound is a tone, a noise burst and a MOD that bends the pitch, plus DIST at the end. This tour visits a few pages. WAVE picks the tone, FILTER shapes the noise, DIST crushes it all.")
page("SEND TO THE PO-32", "Send to PO-32", "Drumlab can send your sounds and patterns to a PO-32 as audio. You plug a cable from the FM-1 output into the PO-32 input. This lesson only opens the window and cancels it, so nothing is sent.")
page("A BEAT FROM ZERO", "On your own", "Time to do it yourself. The cards only name the controls. Make a beat, mark some accents, set a tempo and play it. You know how.")

GWHY = {'Lock a step': 'Normally PITCH and MORPH change the whole sound. In WRITE mode, hold a step and turn the knob: that one step keeps its own pitch or morph. One sound can then play a melody or a one-off accent.', 'Morph a beat': 'Every sound has two versions, patch A and patch B, and KNOB 4 blends between them. Turn it up and hits move toward B. A step you lock keeps its own blend.', 'FX pads': 'Tap FX and the 16 keys become effect pads. Hold one while the beat plays: echo repeats the sound, half slows it, bend bends its pitch. Let go and the effect stops.', 'Record the FX': 'Here you shape one sound with EQ and volume. EQ HZ picks which band of the sound to change. EQ DB boosts or cuts it. LEVEL is how loud the sound is against the others.', 'Punch in live': 'Hold REC while the beat plays and press keys: the hits land on the step nearest the marker. That adds a fill in time without stopping. LFO undoes a take you do not like.', 'Accent size': 'An accent is a louder hit. How much louder is set by VELOC on the SOUND page in LAB: more VELOC, bigger accents. First mark some steps, then set how loud they get.', 'Loop and fine': 'In LAB, PLAY loops the sound so you can hear changes as you turn. Hold EDIT while you turn a knob to move in tiny steps, for fine tuning.', 'Mod modes': 'MOD bends the pitch of the tone. DECAY slides it, SINE wobbles it like a siren, NOISE jitters it. RATE sets how fast. Try each one on a key.', 'Noise shapes': 'The noise burst has a shape. EXP falls away fast, LIN ramps down evenly, MOD flutters like a clap. DECAY sets how long it lasts.', 'EQ and level': 'EQ boosts or cuts one band of the sound. EQ HZ picks the band, EQ DB sets how much, up to 40 dB. LEVEL is the volume of the sound.'}
for _i, (_n, _st, _it) in enumerate(GUIDED): GUIDED[_i] = (_n, _st, [R(_n, GWHY[_n])] + _it)

# ===== the content goes above this line =====

MAXL, WIDTH = 10, 25
def expand(items):
    out = []
    for it in items:
        if not isinstance(it, R): out.append(it); continue
        sents = re.split(r"(?<=[.!?:])\s+", it.body)
        chunks, cur = [], []
        for sn in sents:
            if cur and len(textwrap.wrap(" ".join(cur + [sn]), WIDTH)) > MAXL: chunks.append(" ".join(cur)); cur = []
            cur.append(sn)
        chunks.append(" ".join(cur))
        for k, ch in enumerate(chunks):
            out.append(R(it.title if k == 0 else ("%s %d" % (it.title[:17], k + 1)), ch, it.patch if k == 0 else 0))
    return out
def wrap(body):
    out = []
    for para in body.split("\n"): out += textwrap.wrap(para, WIDTH) or [""]
    return out
def c_str(s): return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'
def steps_c(name, items):
    items = expand(items)
    parts = []
    for it in items:
        if isinstance(it, R):
            lines = wrap(it.body)
            assert len(lines) <= MAXL and len(it.title) <= 20, (name, it.title)
            parts.append('GRD(%s, %s, %d)' % (c_str(it.title), c_str("\n".join(lines)), it.patch))
        else:
            t, v, op, n, a, _ = it
            assert all(len(x) <= 30 for x in t.split('\n')) and t.count('\n') < 2, (name, t, len(t))
            assert 0 <= n <= 255 and 0 <= a <= 255, (name, t)
            parts.append('GSTA(%s, %s, %s, %d, %d)' % (c_str(t), v, op, n, a))
    return len(items), ", ".join(parts)
def actions(items):
    out, cur = [], 0
    for it in expand(items):
        if isinstance(it, R):
            if it.patch: cur = None                           # a patch sets its own page: ALG() is absolute, so the test needs it: patches say pg
            out.append(("read", it.patch)); continue
        act = it[5]
        out.append(act)
    return out
def patch_c(p):
    mc, pid, sel, a, b, side, pg, view, pitch, morph, hits, accents, bpm = p
    trig = [[0] * 16 for _ in range(4)]
    for st, snds in (hits or {}).items():
        for s in snds: trig[s % 4][st] = 1 | (s // 4) << 4
    acc = sum(1 << s for s in accents)
    v = vec(a); bv = vec(b) if b else [0] * 21
    return "    {%d, %d, %d, %d, %d, %s, %d, %d, %d, %d, %d, {%s}, {%s}, {%s}, %d},\n" % (
        mc, pid, sel, side, pg, "V_SND" if view == "LAB" else "V_PAT", bpm, pitch, morph, 1 if b else 0, 1 if hits is not None else 0,
        ", ".join(map(str, v)), ", ".join(map(str, bv)), ", ".join("{%s}" % ", ".join(map(str, r)) for r in trig), acc)
def emit(root):
    src = os.path.join(root, "firmware/src")
    for mid, name, items in MC:
        n, st = steps_c(name, items)
        with open(os.path.join(src, "lessons_mc_%d.h" % mid), "w") as f:
            f.write("/* SPDX-License-Identifier: GPL-3.0-only */\n/* A masterclass, made by tools/masterclasses.py (edit that, not this). */\n    {%s, GS_MC, %d, (const gstep_t[]){%s}, %d, 0},\n" % (c_str(name.upper()), n, st, mid))
    with open(os.path.join(src, "lessons_intro.h"), "w") as f:
        f.write("/* SPDX-License-Identifier: GPL-3.0-only */\n/* The first lessons and the intro pages, made by tools/masterclasses.py (edit that, not this). */\n")
        for name, start, items in INTRO:
            n, st = steps_c(name, items)
            f.write("    {%s, %s, %d, (const gstep_t[]){%s}, 0, %d},\n" % (c_str(name.upper()), "GS_PATCH" if start else "GS_EMPTY", n, st, start))
    with open(os.path.join(src, "lessons_pages.h"), "w") as f:
        f.write("/* SPDX-License-Identifier: GPL-3.0-only */\n/* The page that starts a hand-written lesson, made by tools/masterclasses.py (edit that, not this). */\n")
        for k, (name, r) in enumerate(INTROS.items()):
            n, st = steps_c(name, [r])
            f.write("#define GLI_%d %s,\n" % (k, st))
    with open(os.path.join(src, "lessons_guided.h"), "w") as f:
        f.write("/* SPDX-License-Identifier: GPL-3.0-only */\n/* The guided lessons made from data, by tools/masterclasses.py (edit that, not this). */\n")
        for name, start, items in GUIDED:
            n, st = steps_c(name, items)
            f.write("    {%s, %s, %d, (const gstep_t[]){%s}, 0, %d},\n" % (c_str(name.upper()), "GS_PATCH" if start else "GS_EMPTY", n, st, start))
    with open(os.path.join(src, "lessons_mc_patches.h"), "w") as f:
        f.write("/* SPDX-License-Identifier: GPL-3.0-only */\n/* The patches of the masterclasses, made by tools/masterclasses.py (edit that, not this). */\n")
        for p in PATCHES: f.write(patch_c(p))
if __name__ == "__main__":
    emit(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    print(len(INTRO), "intro lessons,", len(INTROS), "pages,", len(MC), "masterclasses,", len(GUIDED), "guided lessons,", len(PATCHES), "patches")
