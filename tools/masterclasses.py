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
def STOP(text="Press PLAY to stop"): return D(text, "RUN", "EQ", 0, 0, ("tap", "PLAY"))
def KN(n, text, v, op, num, a, clicks): return D(text, v, op, num, a, ("turn", n, clicks))
def ALG(page, text): return D(text, "PG", "EQ", page, 0, ("alg", page))

MC = []                                          # (id, name, items)
GUIDED = []                                      # (name, start patch or 0, items)
PATCHES = []
def masterclass(mid, name, *items): MC.append((mid, name, list(items)))
def guided(name, start, *items): GUIDED.append((name, start, list(items)))
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
  R("Q: resonance", "Q sharpens the filter. On the knob, 20 is a Q of 1, 40 a Q of 10, 60 a Q of 107. LP and HP grow a peak: 9 dB at Q 1, 26 dB at Q 10, 41 dB at Q 100. It gets loud, so mind LEVEL.", patch=7),
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
  R("Half way", "Pitch and time are log scales, so half way is the geometric mean: 50% of 60 and 45 Hz is 52 Hz. 50% of 120 and 800 ms is 311 ms. A drum between two drums, not two at once."),
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
            assert len(t) <= 26, (name, t, len(t))
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
    print(len(MC), "masterclasses,", len(GUIDED), "guided lessons,", len(PATCHES), "patches")
