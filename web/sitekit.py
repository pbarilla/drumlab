#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The pieces every page shares: the masthead (the atom), the radio dial, the footer, and the small helpers the guide and the tutorials use."""
import os, re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SITE = os.path.join(ROOT, "site")
REPO = "https://github.com/pbarilla/drumlab"
try:
    from PIL import Image
except Exception:
    Image = None

NAV = [("Home", ""), ("Play", "play/"), ("Install", "install.html"), ("Manual", "guide/"), ("Tutorials", "manual/"), ("Cheat sheet", "cheatsheet/"), ("Projects", "projects/"), ("Source", None)]


def atom(size=76):
    orbit = "M44,0 A44,15 0 1,1 -44,0 A44,15 0 1,1 44,0"
    rev = "M44,0 A44,15 0 1,0 -44,0 A44,15 0 1,0 44,0"
    o = ['<svg class="atom" viewBox="-50 -50 100 100" width="%d" height="%d" aria-hidden="true">' % (size, size)]
    o.append('<g fill="none" stroke="#6edcb9" stroke-width="2.2">')
    for a in (0, 60, 120):
        o.append('<ellipse rx="44" ry="15" transform="rotate(%d)"/>' % a)
    o.append('</g>')
    for a, d, p, c in ((0, "3.1s", orbit, "#eee4c6"), (60, "4.3s", rev, "#ffa03c"), (120, "5.2s", orbit, "#eee4c6")):
        o.append('<g transform="rotate(%d)"><circle r="3.8" fill="%s"><animateMotion dur="%s" repeatCount="indefinite" path="%s"/></circle></g>' % (a, c, d, p))
    o.append('<circle r="11" fill="#f06446" stroke="#eee4c6" stroke-width="2"/></svg>')
    return "".join(o)


def pills(s):
    """`HOME` -> <code>HOME</code>"""
    return re.sub(r"`([^`]+)`", r"<code>\1</code>", s)


def nav(prefix, active):
    out = []
    for name, href in NAV:
        cur = ' aria-current="page"' if name == active else ""
        if href is None:                                            # the source code: the repository
            out.append('<a href="%s">%s</a>' % (REPO, name))
            continue
        out.append('<a href="%s%s"%s>%s</a>' % (prefix, href or "./", cur, name))
    return '<nav class="dial" aria-label="Pages">%s</nav>' % "".join(out)


def footer(prefix):
    return ('<footer>GPL-3.0-only, see <a href="%s/blob/main/LICENSING.md">LICENSING.md</a>. Built on <a href="%s../chipbox/">Chipbox</a>, which is built on '
            '<a href="https://github.com/zvenson/dinghy">Dinghy</a>, which is built on <a href="https://github.com/hugelton/Felucca">Felucca</a> by Leo Kuroshita '
            '(Hügelton Instruments) with the changes of <a href="https://github.com/isod89/sloop-fm1">SLOOP</a> (isod89). PO-32 and Pocket Operator belong to '
            'Teenage Engineering, who have nothing to do with this.</footer>' % (REPO, prefix))


def page(title, body, depth, active, tag=None, sub=None, desc="", head="", scripts="", main_class="", exp=None):
    p = "../" * depth
    nb = active in ("Tutorials", "Manual", "Install", "Projects", "Cheat sheet")      # every page but the cover and the device is a page from a lab notebook (notebook.css)
    if nb:
        head += '<link rel="stylesheet" href="%snotebook.css">\n' % p
        body = '  <div class="sheet">\n%s%s%s  </div>' % (('  <span class="exp">Experiment %s</span>\n' % exp) if exp else "", body, '  <span class="stamp">Verified</span>\n' if exp else "")
    bodycls = ' class="nb s%d%s"' % ((exp or sum(map(ord, title))) % 4, " corner" if active in ("Install", "Projects", "Cheat sheet") else "") if nb else ""
    t = (' <span class="tag">%s</span>' % tag) if tag else ""
    s = ('<p class="sub">%s</p>' % sub) if sub else ""
    return f'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<meta name="description" content="{desc}">
<link rel="stylesheet" href="{p}lab.css">
{head}</head>
<body{bodycls}>
<main{(' class="%s"' % main_class) if main_class else ""}>
  <div class="mast">{atom()}<h1><a href="{p or './'}">Drumlab</a>{t}</h1></div>
  {s}
  {nav(p, active)}
{body}
  {footer(p)}
</main>
<script src="{p}lab.js"></script>
{scripts}</body>
</html>
'''


# ---- the guide and tutorial building blocks
def img_tag(src_dir, name, alt=""):
    path = os.path.join(SITE, "manual", "img", "pn-" + name + ".png")
    wh = ""
    if Image and os.path.exists(path):
        with Image.open(path) as im:
            wh = ' width="%d" height="%d"' % im.size
    elif not os.path.exists(path):
        print("missing picture", name)
    return '<img src="%simg/pn-%s.png" alt="%s"%s loading="lazy">' % (src_dir, name, alt, wh)


class Doc:
    def __init__(self, img_dir):
        self.img_dir = img_dir
        self.sections = []
        self.n = 0

    def section(self, title, *items, id=None, link=None):
        self.n += 1
        h = '<a href="%s">%s</a>' % (link, title) if link else title
        i = ' id="%s"' % id if id else ""
        self.sections.append('  <section class="step"%s>\n    <h2><b>%d</b>%s</h2>\n%s\n  </section>' % (i, self.n, h, "\n".join(items)))

    def item(self, text, pic=None, alt=""):
        p = '<div class="pic">%s</div>' % img_tag(self.img_dir, pic, alt or pic) if pic else ""
        return '    <div class="item"><div class="t"><div>%s</div></div>%s</div>' % (pills(text), p)

    def block(self, html):
        return '    <div class="block">%s</div>' % html

    def html(self):
        return "\n\n".join(self.sections)


def table(head, rows, cls=""):
    h = "".join("<th>%s</th>" % x for x in head)
    r = "".join("<tr>%s</tr>" % "".join("<td>%s</td>" % pills(c) for c in row) for row in rows)
    return '<table%s><tr>%s</tr>%s</table>' % (' class="%s"' % cls if cls else "", h, r)


def grid(hits, acc=(), names=None, steps=16):
    """the pattern as 16 boxes: hits {sound 1-16: [steps]}, acc [steps]"""
    lanes = {}
    for s, st in hits.items():
        lanes.setdefault((s - 1) & 3, {})[s] = set(st)
    out = ['<div class="gridwrap"><div class="grid" role="img" aria-label="A pattern of 16 steps">', '<span class="h"></span>']
    for i in range(1, steps + 1):
        out.append('<span class="h%s">%d</span>' % (" b" if (i - 1) % 4 == 0 else "", i))
    for lane in range(4):
        sounds = sorted(lanes.get(lane, {}))
        if not sounds and not (names and lane in names): continue
        sd = sounds[0] if sounds else lane + 1
        label = (names or {}).get(lane, "Sound %d" % sd)
        out.append('<span class="n"><i class="l%d">%d</i>%s</span>' % (lane + 1, sd, label))
        for i in range(1, steps + 1):
            on = any(i in lanes.get(lane, {}).get(s, ()) for s in sounds)
            cls = "c" + (" on l%d" % (lane + 1) if on else "") + (" g" if (i - 1) % 4 == 0 and i > 1 else "") + (" acc" if on and i in acc else "")
            out.append('<span class="%s"></span>' % cls)
    out.append('</div></div>')
    return "".join(out)
