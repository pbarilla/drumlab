#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The FM-1 panel as the Play online page draws it, with a control ringed and a screen in it. Needs Pillow."""
from PIL import Image, ImageDraw, ImageFont
X0, Y0, W, H = 30, 82, 1040, 664                 # the page's viewBox
KNOBS = {"MASTER": (139, 187), "SELECT": (253, 185), "PRESETS": (139, 295), "ALGORITHM": (253, 295),
         "KNOB1": (665, 185), "KNOB2": (772, 185), "KNOB3": (880, 185), "KNOB4": (988, 185)}
BTNS = {"OCT-": (147, 380, 84), "OCT+": (249, 380, 84), "FX": (663, 287, 58), "SEL": (729, 287, 58), "ENV": (794, 287, 58), "LFO": (860, 287, 58),
        "EDIT": (925, 287, 58), "GLO": (990, 287, 58), "HOME": (663, 352, 58), "SAVE": (729, 352, 58), "ARP": (794, 352, 58), "SEQ": (860, 352, 58),
        "PLAY": (925, 352, 58), "REC": (990, 352, 58)}
BLACK = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25}
BG, PLATE, RAIL, CREAM, CORAL, AMB = (26, 24, 34), (38, 36, 48), (46, 44, 58), (238, 228, 198), (240, 100, 70), (255, 160, 60)
FONT = "/System/Library/Fonts/Menlo.ttc"


def font(sz):
    try:
        return ImageFont.truetype(FONT, sz, index=1)
    except Exception:
        return ImageFont.load_default()


def key_box(k):
    wi = sum(1 for j in range(k) if j not in BLACK)
    if k in BLACK:
        cx = 109 + (wi - 0.5) * 58.7
        return (cx - 23, 452, cx + 23, 546)
    cx = 109 + wi * 58.7
    return (cx - 23, 553, cx + 23, 701)


def panel(high=(), screen=None, keys=(), width=960, lit=(), crop=True):
    """high: controls to ring; screen: a PIL image; keys: key numbers to colour amber; lit: buttons / keys shown on (coral)."""
    S = 2.0 * width / W                            # draw at twice the size, then shrink
    im = Image.new("RGB", (int(W * S), int(H * S)), BG)
    d = ImageDraw.Draw(im)
    P = lambda x, y: ((x - X0) * S, (y - Y0) * S)
    def rect(x1, y1, x2, y2, r, fill, outline=None, w=0):
        a, b = P(x1, y1), P(x2, y2)
        d.rounded_rectangle([a, b], radius=r * S, fill=fill, outline=outline, width=int(w * S))
    rect(32, 84, 1068, 744, 46, PLATE, (74, 70, 90), 4)
    rect(342, 127, 588, 373, 6, (10, 9, 14))
    rect(90, 349, 308, 408, 10, BG); rect(621, 247, 1037, 393, 14, BG); rect(67, 435, 1033, 715, 14, BG)
    if screen is not None:
        sc = screen.convert("RGB").resize((int(208 * S), int(208 * S)), Image.NEAREST)
        im.paste(sc, tuple(int(v) for v in P(361, 145)))
    else:
        rect(361, 145, 569, 353, 2, (0, 0, 0))
    f = font(int(15 * S))
    for name, (cx, cy) in KNOBS.items():
        a, b = P(cx - 29, cy - 29), P(cx + 29, cy + 29)
        d.ellipse([a, b], fill=BG, outline=(74, 70, 90), width=int(4 * S))
        d.line([P(cx, cy - 6), P(cx, cy - 22)], fill=CREAM, width=int(5 * S))
        tw = d.textlength(name, font=f)
        d.text((P(cx, 0)[0] - tw / 2, P(0, cy - 53)[1]), name, font=f, fill=(150, 146, 160))
    for name, (cx, cy, w) in BTNS.items():
        on = name in lit
        rect(cx - w / 2, cy - 21, cx + w / 2, cy + 21, 9, CORAL if on else RAIL, (74, 70, 90), 2)
        tw = d.textlength(name, font=f)
        d.text((P(cx, 0)[0] - tw / 2, P(0, cy)[1] - 8 * S), name, font=f, fill=(255, 255, 255) if on else (205, 209, 216))
    for k in range(27):
        x1, y1, x2, y2 = key_box(k)
        rect(x1, y1, x2, y2, 23, CORAL if k in lit else ((70, 66, 84) if k in BLACK else (92, 88, 106)))
    for k in keys:
        x1, y1, x2, y2 = key_box(k)
        rect(x1, y1, x2, y2, 23, AMB)
    boxes = []
    for h in high:
        if h in KNOBS:
            cx, cy = KNOBS[h]; boxes.append((cx - 46, cy - 46, cx + 46, cy + 46, 46))
        elif h in BTNS:
            cx, cy, w = BTNS[h]; boxes.append((cx - w / 2 - 12, cy - 33, cx + w / 2 + 12, cy + 33, 16))
        elif h == "keys":
            boxes.append((60, 428, 1040, 722, 18))
        elif h == "screen":
            boxes.append((336, 121, 594, 379, 12))
    for x1, y1, x2, y2, r in boxes:
        rect(x1, y1, x2, y2, r, None, AMB, 7)
    out = im.resize((width, int(H * width / W)), Image.LANCZOS)
    if crop:                                        # the top of the panel only: knobs, buttons, screen
        out = out.crop((0, 0, width, int((424 - Y0) * width / W)))
    return out
