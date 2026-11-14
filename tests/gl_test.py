#!/usr/bin/env python3
"""Runs every guided lesson on the host sim: starts it from the GLO + HOME menu, does what its cards say, and checks that each step finishes.
   python3 tests/gl_test.py build/host/lab_gl [lesson numbers]"""
import os, subprocess, sys, tempfile, re
sys.path.insert(0, os.path.dirname(__file__))
from lab_gen import S, SELECT, ALGO, K1, K2, K3, K4, PRESETS
_turn = S.turn
def _turn4(s, enc, d):                                          # a knob step wants a few clicks
    for _ in range(-(-4 // abs(d)) if 2 <= enc <= 5 and abs(d) < 4 else 1): _turn(s, enc, d)
S.turn = _turn4
NL = 0  # no ghost lessons any more
def W(s, ms=1000): s.wait(ms)
def A_get(s):
    for _ in range(3): s.tap('HOME'); W(s)
def A_pad(s): s.key(0); W(s); s.turn(PRESETS, 1); W(s); s.key(1)
def A_first(s):
    s.tap('REC'); W(s)
    for k in (0, 4, 8, 12): s.key(k)
    W(s); s.turn(PRESETS, 1); W(s)
    for k in (4, 12, 8): s.key(k)
    W(s); s.tap('PLAY')
def A_sounds(s):
    s.turn(PRESETS, 1); W(s)
    for k in (0, 1, 2): s.key(k)
    W(s); s.tap('PLAY')
def A_acc(s):
    s.tap('ENV'); W(s)
    for k in (1, 3): s.key(k)
    W(s); s.tap('ENV'); W(s); s.tap('PLAY')
def A_tempo(s):
    s.tap('PLAY'); W(s); s.turn(K1, 9); W(s); s.turn(K2, 4); W(s); s.tap('PLAY')
def A_pitch(s): s.turn(K3, 2); W(s); s.turn(K4, 2); W(s); s.key(0)
def A_pat(s):
    s.turn(SELECT, 1); W(s)
    for k in (1, 2, 3): s.key(k)
    W(s); s.tap('OCTUP'); W(s); s.turn(SELECT, -2)
def A_chain(s):
    s.tap('SEL'); W(s); s.key(0); s.key(1); W(s); s.tap('SEL'); W(s); s.tap('PLAY'); W(s); s.tap('PLAY')
def A_mute(s):
    s.tap('PLAY'); W(s); s.tap('ARP'); W(s); s.key(0); W(s); s.tap('ARP'); W(s); s.tap('ARP')
def A_fx(s):
    s.tap('PLAY'); W(s); s.tap('FX'); W(s); s.keydown(0); W(s, 300); s.keyup(); W(s); s.tap('FX')
def A_undo(s):
    for k in (1, 2, 3): s.key(k)
    W(s, 1500); s.tap('LFO'); W(s, 1500); s.press('EDIT'); W(s, 900); s.release('EDIT')
def A_lab(s): s.tap('HOME'); W(s); s.turn(ALGO, 1); W(s); s.turn(K1, 3); W(s); s.key(0)
def A_ab(s):
    s.tap('HOME'); W(s); s.tap('SEL'); W(s); s.turn(K1, 3); W(s); s.tap('SEL'); W(s); s.tap('HOME'); s.tap('HOME'); W(s); s.turn(K4, 2)
def A_kit(s): s.tap('HOME'); W(s); s.tap('OCTUP'); W(s, 400); s.turn(SELECT, 1); W(s, 300); s.tap('OCTUP')
def A_send(s): s.tap('HOME'); s.tap('HOME'); W(s); s.tap('PLAY'); W(s); s.tap('OCTDN')
def A_zero(s):
    s.tap('REC'); W(s)
    for k in (0, 4, 8, 12): s.key(k)
    s.turn(PRESETS, 1)
    for k in (4, 12, 2, 10): s.key(k)
    W(s); s.tap('ENV'); W(s); s.key(0); s.key(4); W(s); s.tap('ENV'); s.turn(K1, 2); W(s); s.tap('PLAY')
def A_engines(s):
    s.tap('HOME'); W(s); s.turn(K1, 1); W(s); s.turn(ALGO, 1); W(s); s.turn(K3, 3); W(s); s.turn(ALGO, 1); W(s); s.turn(K1, 3); W(s); s.turn(ALGO, 2); W(s); s.turn(K1, 3); W(s); s.key(0)
ACTIONS = [A_sounds, A_acc, A_tempo, A_pitch, A_pat, A_chain, A_mute, A_fx, A_undo, A_lab, A_ab, A_kit, A_engines, A_send, A_zero]
NAMES18 = ["SOUNDS", "ACCENTS", "TEMPO AND SWING", "PITCH AND MORPH", "PATTERNS", "CHAIN PATTERNS", "MUTE AND SOLO", "PUNCH-IN FX", "UNDO AND CLEAR", "SOUND LAB",
           "PATCH A AND B", "ROLL A KIT", "SOUND ENGINES", "SEND TO THE PO-32", "A BEAT FROM ZERO"]
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'tools'))
import masterclasses
ENC = dict(K1=K1, K2=K2, K3=K3, K4=K4, SELECT=SELECT, ALGO=ALGO, PRESETS=PRESETS)
def data_action(items):                                           # a lesson made from data (tools/masterclasses.py)
    acts = masterclasses.actions(items)
    def run(s):
        for a in acts:
            if a[0] == 'read': s.tap('OCTUP')
            elif a[0] == 'tap': s.tap(a[1])
            elif a[0] == 'key': s.key(a[1])
            elif a[0] == 'turn':
                enc = ENC[a[1]] if isinstance(a[1], str) else [K1, K2, K3, K4][a[1] - 1]
                for _ in range(abs(a[2])): s.L.append(f"{s.t} enc {enc} {1 if a[2] > 0 else -1}"); s.t += 60
            elif a[0] == 'alg':
                for _ in range(abs(a[1])): s.turn(ALGO, 1 if a[1] > 0 else -1); W(s, 150)
            elif a[0] == 'hold': s.press(a[1]); W(s, a[2]); s.release(a[1])
            elif a[0] == 'py': exec(a[1], dict(globals(), s=s))
            W(s, 1300)
    return run
ACTION_BY_NAME = dict(zip(NAMES18, ACTIONS))
ACTION_BY_NAME.update({name.upper(): data_action(items) for _, name, items in masterclasses.MC})
ACTION_BY_NAME.update({name.upper(): data_action(items) for name, _, items in masterclasses.GUIDED + masterclasses.INTRO})
def _pre(f):
    def run(s): s.tap('OCTUP'); W(s, 1300); f(s)
    return run
for _n in masterclasses.INTROS: ACTION_BY_NAME[_n] = _pre(ACTION_BY_NAME[_n])
def lessons_of():                                                 # (name, steps) of every guided lesson, in order
    src = open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'firmware/src/lablesson.c')).read()
    body = src[src.index('static const glesson_t GLESSON[]'):src.index('static const char *const GL_PRAISE')]
    body = re.sub(r'#include "(lessons_\w+\.h)"', lambda m: open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'firmware/src', m.group(1))).read(), body)
    return [(m.group(1), int(m.group(2))) for m in re.finditer(r'\{"([^"]+)", GS_\w+, (\d+), \(const gstep_t\[\]\)\{', body)]
LESSONS = lessons_of()
STEPS = [n for _, n in LESSONS]
def menu(s, i):
    s.press('GLO'); s.press('HOME'); s.release('HOME'); s.release('GLO'); W(s, 300)
    s.tap('OCTUP'); W(s, 400)                                   # the row opens the list
    for _ in range(i if i < 2 else NL + i): s.turn(SELECT, 1); W(s, 60)
    W(s, 100); s.tap('OCTUP'); W(s, 600)
def run(binary, i):
    s = S(); menu(s, i); ACTION_BY_NAME[LESSONS[i][0]](s); W(s, 1500)
    d = tempfile.mkdtemp(); f = os.path.join(d, 's.txt'); s.save(f)
    r = subprocess.run([binary, os.path.join(d, 'o'), f], capture_output=True, text=True, timeout=900, env=dict(os.environ, HOST_FLASH=os.path.join(d, 'f.bin')))
    for m in re.finditer(r'GLESSON (\d+) STEP (\d+) INSTANT', r.stdout):
        if int(m.group(1)) == i: print(f'   INSTANT: lesson {i + 1} step {m.group(2)} is already true when it shows')
    got = sorted({int(m.group(2)) for m in re.finditer(r'GLESSON (\d+) STEP (\d+) OK', r.stdout) if int(m.group(1)) == i})
    ok = got == list(range(STEPS[i]))
    print(('ok   ' if ok else 'FAIL ') + f'lesson {i + 1} {LESSONS[i][0]}: steps {len(got)} of {STEPS[i]}' + ('' if ok else f'  MISSING {sorted(set(range(STEPS[i])) - set(got))}'))
    return ok
if __name__ == '__main__':
    b = sys.argv[1]; sel = [int(x) - 1 for x in sys.argv[2:]] or range(len(LESSONS))
    missing = [n for n, _ in LESSONS if n not in ACTION_BY_NAME]; assert not missing, missing
    sys.exit(0 if all([run(b, i) for i in sel]) else 1)
