#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The sample projects of the Projects page (site/projects/samples/*.drumlab + samples.json). The bytes come from the firmware's own
lab_pack (web/dump_samples.c); a Python port of lab_unpack reads every file back and checks it against patterns.py.
   python3 web/make_samples.py"""
import json, os, re, struct, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from patterns import PATTERNS, SAMPLES, by_name, trig

OUT = os.path.join(ROOT, "site", "projects", "samples")
NS, NPAT, DR, CHAIN_MAX = 16, 16, 21, 64
PAT_SZ = 64 + 128 + 2 + 32                                  # lpat_t
PAT0 = 5 + CHAIN_MAX + 2 * NS + 2 * NS * DR * 2             # LAB_PAT0


def kit_names():
    s = open(os.path.join(ROOT, "firmware", "gen", "arch.h")).read()
    return re.findall(r'\{"([A-Z0-9 -]+)", \{\{', s)


def unpack(p):
    """a port of lab_unpack: the project bytes -> a dict (raises on anything the firmware would refuse)"""
    if p[0] < 1 or p[0] > 3: raise ValueError("version")
    psz = 194 if p[0] == 1 else PAT_SZ
    if len(p) < PAT0 + NPAT * psz: raise ValueError("short")
    o = {"ver": p[0], "bpm": p[1], "swing": min(p[2], 12), "cur": p[3] & 15, "chain": [c & 15 for c in p[5:5 + min(p[4], CHAIN_MAX)]]}
    n = 5 + CHAIN_MAX
    o["knob"] = [(p[n + 2 * s], p[n + 2 * s + 1]) for s in range(NS)]; n += 2 * NS
    o["kp"] = [[struct.unpack_from("<%dH" % DR, p, n + (s * 2 + q) * DR * 2) for q in range(2)] for s in range(NS) for _ in [0]]
    n += NS * 2 * DR * 2
    o["pat"] = []
    for i in range(NPAT):
        b = p[n:n + psz]; n += psz
        o["pat"].append({"trig": [list(b[l * 16:l * 16 + 16]) for l in range(4)], "lock": b[64:192], "acc": b[192] | b[193] << 8})
    o["snm"] = list(p[n:n + NS]) if p[0] >= 3 and len(p) >= n + NS else [0] * NS
    if any(v > 32 for v in o["snm"]): o["snm"] = [0] * NS
    return o


def main():
    kits = kit_names()
    os.makedirs(OUT, exist_ok=True)
    spec, names = [], []
    for title, kit, pats, chain in SAMPLES:
        k = -1 if kit == "LAB-1" else kits.index(kit)
        fn = re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-") + ".drumlab"
        bpm = by_name(pats[0])[1]; sw = by_name(pats[0])[2]
        spec.append("kit %d\nbpm %d\nswing %d\nchain %d %s" % (k, bpm, sw, len(chain), " ".join(map(str, chain))))
        for i, n in enumerate(pats):
            t, a = trig(by_name(n))
            spec.append("pat %d %d %s" % (i, a, " ".join(str(v) for row in t for v in row)))
        spec.append("out " + os.path.join(OUT, fn))
        names.append((title, kit, pats, chain, fn, bpm, sw))
    tmp = tempfile.mkdtemp(prefix="dl-")
    sp, exe = os.path.join(tmp, "spec.txt"), os.path.join(tmp, "dump")
    open(sp, "w").write("\n".join(spec) + "\n")
    subprocess.run(["cc", "-O2", "-DCHIPBOX_HOST", "-Dmain=sim_main", "-I" + os.path.join(ROOT, "firmware/src"), "-I" + os.path.join(ROOT, "firmware/gen"),
                    "-o", exe, os.path.join(HERE, "dump_samples.c")], check=True, cwd=ROOT)
    subprocess.run([exe, sp], check=True, cwd=ROOT)
    lst = []
    for title, kit, pats, chain, fn, bpm, sw in names:
        raw = open(os.path.join(OUT, fn), "rb").read()
        o = unpack(raw)                                                       # the round trip: decode our own file
        assert o["ver"] == 3 and o["bpm"] == bpm and o["swing"] == sw and o["chain"] == chain, fn
        assert len(raw) == PAT0 + NPAT * PAT_SZ + NS, (fn, len(raw))
        for i, n in enumerate(pats):
            t, a = trig(by_name(n))
            assert o["pat"][i]["trig"] == t and o["pat"][i]["acc"] == a, (fn, n)
        assert all(v == 0 for p in o["pat"][len(pats):] for r in p["trig"] for v in r)
        assert any(o["kp"][s][0][1] for s in range(NS)), fn                   # real sound data came along
        print("ok", fn, len(raw), "B", kit, "%d bpm" % bpm)
        lst.append({"name": title, "file": "samples/" + fn, "note": "%s kit, %d bpm" % (kit, bpm)})
    json.dump(lst, open(os.path.join(ROOT, "site", "projects", "samples.json"), "w"), indent=1)


if __name__ == "__main__":
    main()
