#!/usr/bin/env python3
"""Writes tests/lab_*.txt scripts for the host sim: a tiny language over the panel (press, tap, keys, turn)."""
W=[0,2,4,6,7,9,11,12,14,16,18,19,21,23,24,26]
B=dict(OCTDN=0,OCTUP=1,FX=2,SEL=3,ENV=4,LFO=5,EDIT=6,GLO=7,HOME=8,SAVE=9,ARP=10,SEQ=11,PLAY=12,REC=13)
class S:
    def __init__(s): s.L=[]; s.t=300; s.held=0
    def wait(s,ms): s.t+=ms
    def tap(s,b,hold=60):
        s.held|=1<<B[b]; s.L.append(f"{s.t} held {s.held:#x}"); s.t+=hold
        s.held&=~(1<<B[b]); s.L.append(f"{s.t} held {s.held:#x}"); s.t+=60
    def press(s,b): s.held|=1<<B[b]; s.L.append(f"{s.t} held {s.held:#x}"); s.t+=20
    def release(s,b): s.held&=~(1<<B[b]); s.L.append(f"{s.t} held {s.held:#x}"); s.t+=60
    def key(s,i,hold=50): s.L.append(f"{s.t} keys {1<<W[i]:#x}"); s.t+=hold; s.L.append(f"{s.t} keys 0"); s.t+=50
    def keydown(s,i): s.L.append(f"{s.t} keys {1<<W[i]:#x}"); s.t+=20
    def keyup(s): s.L.append(f"{s.t} keys 0"); s.t+=50
    def turn(s,enc,d): s.L.append(f"{s.t} enc {enc} {d}"); s.t+=40
    def shot(s): s.L.append(f"{s.t} shot"); s.t+=60
    def save(s,path): s.L.append(f"{s.t} end"); open(path,'w').write("\n".join(s.L)+"\n")
SELECT,ALGO,K1,K2,K3,K4,PRESETS=0,1,2,3,4,5,6
def beat(s):
    s.tap('REC')
    for st in (0,4,8,12): s.key(st)
    s.turn(PRESETS,1)
    for st in (4,12): s.key(st)
    s.turn(PRESETS,1)
    for st in (2,6,10,14): s.key(st)
    s.turn(PRESETS,4)
    for st in (1,3,5,7,9,11,13,15): s.key(st)
    s.tap('REC')
