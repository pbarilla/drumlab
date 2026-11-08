// SPDX-License-Identifier: GPL-3.0-only
// Microtonic files <-> Drumlab. Written for this project; the value mapping follows the PO-32 patch maths that libpo (MIT, Eric Lewis,
// github.com/ericlewis/libpo, src/core/src/po32_patch_import.c) documents, and tests/test_mtimport.mjs checks every number against libpo's C.
//
// WHAT IS READ (plain text files, every version Microtonic made):
//   .mtpreset  a preset: 8 drum patches + 12 patterns (a-l) + tempo, step rate, swing   (V3: "MicrotonicPresetV3: {", also the older "=" spelling, and V1 ".mtpg")
//   .mtdrum    one drum patch                                                           (V3 "MicrotonicDrumPatchV3: {", V1 ".mtdp", or bare "Key: value" lines)
//   .mtpg      the old program file (V1: "MicroTonicProgramV1={") or a V3 preset with that extension
//   .mid       a pattern dragged out of Microtonic (its "drag to MIDI" icon): notes 36-43 (C1-G1) are drums 1-8. Microtonic has no other single-pattern file.
// Values come with units ("47.1 Hz", "+30.9 sm", "94.9 / 5.0") or without ("47.1"). Mix is always the oscillator's share first.
//
// A DRUM PATCH becomes 21 u16 in the PO-32's wire order (Mix stored as the noise share, 0 = oscillator only).
// A PATTERN: Drumlab has 4 lanes, sound % 4 is the lane, and a step holds one hit per lane. Drums 1-8 land on 8 sounds (sounds 1-8 or 9-16), so
// drums 1 and 5, 2 and 6, 3 and 7, 4 and 8 share a lane. See mapPatterns() for the exact rules and what is lost.

import { NS, NPAT, NP, PITCH_MID, CHAIN_MAX, newPattern, clampBpm } from "./drumproj.js";

export const MAX_BYTES = 1 << 20;                 // a preset is 10-50 KB; anything over 1 MB is not one
const MAX_LINES = 40000, MAX_LINE = 4096, MAX_DEPTH = 12;
export const LANES = 4;

const fail = (m) => { const e = new Error(m); e.code = "badfile"; return e; };
const f32 = Math.fround;
const c01 = (x) => (x < 0 || x !== x ? 0 : x > 1 ? 1 : x);

// ======================================================================= the text tree
// A line is "key: value", "key = value", "key: {" (opens a block), "}" (closes), or "key: { a, b, c }" (a list). Keys may be quoted or look like Channel[0].
const LINE = /^"?([^"{}:=\s][^"{}:=]*?)"?\s*[:=]\s*(.*)$/;
function clean(v) {
  v = v.trim();
  if (v.endsWith(",")) v = v.slice(0, -1).trim();
  if (v.length >= 2 && v[0] === '"' && v[v.length - 1] === '"') v = v.slice(1, -1).replace(/\\(.)/g, "$1").trim();
  return v;
}
export function parseTree(text) {
  const root = { key: "", props: Object.create(null), kids: [] };
  const stack = [root];
  let n = 0;
  for (const raw of text.split("\n")) {
    if (++n > MAX_LINES) throw fail("That file is too long to be a Microtonic file.");
    if (raw.length > MAX_LINE) continue;
    const line = raw.trim();
    if (!line || line.startsWith("//") || line.startsWith("#")) continue;
    if (line === "}" || line === "},") { if (stack.length > 1) stack.pop(); continue; }
    const m = LINE.exec(line);
    if (!m) continue;
    const key = m[1].trim(), rest = m[2].trim();
    const cur = stack[stack.length - 1];
    if (rest === "{") {
      if (stack.length > MAX_DEPTH) throw fail("That file is nested too deep to be a Microtonic file.");
      const node = { key, props: Object.create(null), kids: [] };
      cur.kids.push(node); stack.push(node);
    } else if (rest.startsWith("{") && rest.endsWith("}")) {
      cur.props[key] = rest.slice(1, -1).split(",").map((s) => clean(s)).filter((s) => s.length);
    } else cur.props[key] = clean(rest);
  }
  return root;
}
const kid = (node, re) => node.kids.find((k) => re.test(k.key));

// ======================================================================= one drum patch: text -> 21 u16
const NUM = /^[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?/;
const num = (s) => { const m = s == null ? null : NUM.exec(s.trim()); return m ? Number(m[0]) : NaN; };
const isInf = (s) => /^[+-]?inf/i.test(s.trim());
const log10 = Math.log10;

const hzToParam = (hz) => c01(Math.log(hz / 20) / Math.log(1000));
const decayToParam = (ms) => c01((log10(ms / 1000) + 2) / 3);
const attackSeconds = (x) => (x <= 0 ? 0 : x * 0.03981071706 * Math.pow(10, 2.4 * x));
function attackToParam(ms) {
  if (!(ms > 0)) return 0;
  const sec = ms / 1000;
  let lo = 0, hi = 1;
  for (let i = 0; i < 48; i++) { const mid = (lo + hi) / 2; if (attackSeconds(mid) < sec) lo = mid; else hi = mid; }
  return c01((lo + hi) / 2);
}
const qToParam = (q) => c01(log10(q / 0.1) / 5);
const modFromSemitones = (sm, decay) => { const lim = decay ? 96 : 48; return 0.5 + (sm < 0 ? -0.5 : 0.5) * Math.sqrt(c01(Math.abs(sm) / lim)); };
const modFromMs = (ms) => c01(1.03 / (1 + 0.003 * ms));
const modHz = (x, sine) => (sine ? 2000 : 20000) * x * Math.pow(10, (sine ? 3.5 : 4) * (x - 1));
function modFromHz(hz, sine) {
  if (hz <= 0) return 0;
  if (hz >= (sine ? 2000 : 20000)) return 1;
  let lo = 0, hi = 1;
  for (let i = 0; i < 48; i++) { const mid = (lo + hi) / 2; if (modHz(mid, sine) < hz) lo = mid; else hi = mid; }
  return (lo + hi) / 2;
}
function levelToParam(db) {
  if (db <= -500) return 0;
  let lo = 0.0001, hi = 1;
  for (let i = 0; i < 48; i++) { const mid = (lo + hi) / 2; if (60 * mid - 49 - 1 / mid < db) lo = mid; else hi = mid; }
  return c01((lo + hi) / 2);
}
// the PO-32's own rounding: (uint16)(x * 65535 + 0.5f) in single precision
export const toU16 = (x) => Math.floor(f32(f32(c01(f32(x)) * 65535) + 0.5));

const WAVES = [["sine", 0], ["triangle", 0.5], ["saw", 1]];
const FMODS = [["lp", 0], ["bp", 0.5], ["hp", 1]];
const EMODS = [["exp", 0], ["lin", 0.5], ["mod", 1]];
const MMODS = [["decay", 0], ["sine", 0.5], ["noise", 1]];
const pickMode = (v, table) => { const s = (v || "").trim().toLowerCase(); for (const [k, x] of table) if (s.startsWith(k)) return x; return null; };

// props: key -> text. Returns { p: Uint16Array(21), warn: [..] }. A field it can't read is left at 0 (as libpo leaves it) and named in warn.
export function drumFromProps(pr) {
  const x = new Array(NP).fill(0), warn = [];
  const bad = (k) => warn.push(k);
  const need = (k, fn) => { if (pr[k] == null) return; const r = fn(pr[k]); if (r == null || r !== r) bad(k); else return r; };
  // the mode selectors first (the units of other fields depend on them)
  const mm = pr.ModMode == null ? null : pickMode(pr.ModMode, MMODS);
  if (pr.ModMode != null && mm == null) bad("ModMode");
  const em = pr.NEnvMod == null ? null : pickMode(pr.NEnvMod, EMODS);
  if (pr.NEnvMod != null && em == null) bad("NEnvMod");
  const fm = pr.NFilMod == null ? null : pickMode(pr.NFilMod, FMODS);
  if (pr.NFilMod != null && fm == null) bad("NFilMod");
  const wv = pr.OscWave == null ? null : pickMode(pr.OscWave, WAVES);
  if (pr.OscWave != null && wv == null) bad("OscWave");
  x[0] = wv || 0; x[4] = mm == null ? 0 : mm; x[7] = fm || 0; x[10] = em == null ? 0 : em;
  const pos = (s) => { const v = num(s); return v > 0 ? v : null; };
  const f = (k, i, fn) => { const r = need(k, fn); if (r != null) x[i] = r; };
  f("OscFreq", 1, (s) => { const v = pos(s); return v == null ? null : hzToParam(v); });
  f("OscAtk", 2, (s) => { const v = num(s); return v !== v ? null : attackToParam(v); });
  f("OscDcy", 3, (s) => { const v = pos(s); return v == null ? null : decayToParam(v); });
  f("ModRate", 5, (s) => {
    if (/ms/.test(s) || mm === 0) { if (isInf(s)) return 0; const v = pos(s); return v == null ? null : modFromMs(v); }
    if (isInf(s)) return 0;
    const v = num(s); if (!(v >= 0)) return null;
    return mm === 0.5 ? modFromHz(v, true) : v === 0 ? 0 : modFromHz(v, false);
  });
  f("ModAmt", 6, (s) => { const v = num(s); return v !== v ? null : modFromSemitones(v, mm == null || mm === 0); });
  f("NFilFrq", 8, (s) => { const v = pos(s); return v == null ? null : hzToParam(v); });
  f("NFilQ", 9, (s) => { const v = pos(s); return v == null ? null : qToParam(v); });
  const lin = em === 0.5 ? 1.5 : 1;
  f("NEnvAtk", 11, (s) => { const v = num(s); return v !== v ? null : attackToParam(v * lin); });
  f("NEnvDcy", 12, (s) => { const v = pos(s); return v == null ? null : decayToParam(v * lin); });
  f("Mix", 13, (s) => { const v = num(s); return v !== v ? null : c01(1 - v / 100); });
  f("DistAmt", 14, (s) => { const v = num(s); return v !== v ? null : c01(v / 100); });
  f("EQFreq", 15, (s) => { const v = pos(s); return v == null ? null : hzToParam(v); });
  f("EQGain", 16, (s) => { const v = num(s); return v !== v ? null : c01((v + 40) / 80); });
  f("Level", 17, (s) => { if (/^[+-]?inf/i.test(s.trim())) return 0; const v = num(s); return v !== v ? null : levelToParam(v); });
  for (const [k, i] of [["OscVel", 18], ["NVel", 19], ["ModVel", 20]]) f(k, i, (s) => { const v = num(s); return v !== v ? null : c01(v / 200); });
  const p = new Uint16Array(NP);
  for (let i = 0; i < NP; i++) p[i] = toU16(x[i]);
  return { p, warn };
}
const KEYS = ["OscWave", "OscFreq", "OscAtk", "OscDcy", "ModMode", "ModRate", "ModAmt", "NFilMod", "NFilFrq", "NFilQ", "NEnvMod", "NEnvAtk", "NEnvDcy", "Mix", "DistAmt", "EQFreq", "EQGain", "Level", "OscVel", "NVel", "ModVel"];
const looksDrum = (pr) => KEYS.some((k) => pr[k] != null);

// ======================================================================= patterns
const ON = /^[#Xx1*]$/;
const rowBits = (s, bit) => { const out = new Uint8Array(16); if (typeof s !== "string") return out; for (let i = 0; i < 16 && i < s.length; i++) if (ON.test(s[i])) out[i] = bit; return out; };
const STEP_RATES = { "1/8": 2, "1/8t": 3, "1/16": 4, "1/16t": 6, "1/32": 8 };    // steps per quarter note

function readPatterns(node) {
  const holder = kid(node, /^patterns$/i);
  const list = holder ? holder.kids : node.kids.filter((k) => /^pattern\[\d+\]$/i.test(k.key));
  const out = [];
  const chainedV1 = Array.isArray(node.props.Chained) ? node.props.Chained.map((v) => /^(true|on|1)$/i.test(v)) : [];
  list.slice(0, 12).forEach((pn, i) => {
    const ch = [];
    const chNodes = pn.kids.filter((k) => /^(\d|channel\[\d\])$/i.test(k.key)).slice(0, 8);
    for (let d = 0; d < 8; d++) {
      const c = chNodes[d], t = new Uint8Array(16);
      if (c) { const tr = rowBits(c.props.Triggers, 1), ac = rowBits(c.props.Accents, 2), fl = rowBits(c.props.Fills, 4); for (let s = 0; s < 16; s++) t[s] = tr[s] | ac[s] | fl[s]; }
      ch.push(t);
    }
    const len = Math.round(num(pn.props.Length));
    out.push({ steps: len >= 1 && len <= 16 ? len : 16, chained: /^(true|on|1)$/i.test(pn.props.Chained || "") || !!chainedV1[i], ch });
  });
  return out;
}

// ======================================================================= a whole file
// Returns { kind: "preset" | "drum" | "midi", name, drums: [{ name, p, warn }], patterns: [...], tempo, swing (0..100), stepRate, mutes, current, notes: [] }
export function parseMicrotonic(input, fileName) {
  try { return parseAny(input, fileName); }
  catch (e) { throw e && e.code === "badfile" ? e : fail("That file is broken."); }
}
function parseAny(input, fileName) {
  if (!(input instanceof Uint8Array)) throw fail("That isn't a file Drumlab can read.");
  if (input.length > MAX_BYTES) throw fail("That file is too big to be a Microtonic file.");
  if (input.length >= 4 && input[0] === 0x4d && input[1] === 0x54 && input[2] === 0x68 && input[3] === 0x64) return parseMidi(input, fileName);
  let bin = 0;
  for (let i = 0; i < Math.min(input.length, 4096); i++) if (input[i] === 0) bin++;
  if (bin) throw fail("That isn't a text file, so it isn't a Microtonic file.");
  const text = new TextDecoder("utf-8").decode(input).replace(/^﻿/, "").replace(/\r\n?/g, "\n");
  const tree = parseTree(text);
  const base = String(fileName || "").replace(/^.*[\\/]/, "").replace(/\.[^.]*$/, "");
  const head = tree.kids.find((k) => /^microtonic/i.test(k.key));
  if (head && /preset|program/i.test(head.key)) return readPreset(head, base);
  if (head && /drum/i.test(head.key)) return readOneDrum(head.props, base);
  if (looksDrum(tree.props)) return readOneDrum(tree.props, base);
  throw fail("That isn't a Microtonic preset or drum patch.");
}
function readOneDrum(props, base) {
  const d = drumFromProps(props);
  return { kind: "drum", name: props.Name || base || "Drum", drums: [{ name: props.Name || base || "Drum", p: d.p, warn: d.warn }], patterns: [], tempo: null, swing: 0, stepRate: "1/16", mutes: [], current: 0, notes: [] };
}
function readPreset(node, base) {
  const holder = kid(node, /^drumpatches$/i);
  const dn = (holder ? holder.kids : node.kids.filter((k) => /^channel\[\d+\]$/i.test(k.key))).slice(0, 8);
  const drums = dn.map((k, i) => { const d = drumFromProps(k.props); return { name: k.props.Name || `Drum ${i + 1}`, p: d.p, warn: d.warn }; });
  if (!drums.length) throw fail("That preset has no drum patches in it.");
  const patterns = readPatterns(node);
  const rate = String(node.props.StepRate || "1/16").trim().toLowerCase();
  const letter = String(node.props.Pattern || "a").trim().toLowerCase();
  const mutes = Array.isArray(node.props.Mutes) ? node.props.Mutes.map((v) => /^(on|true|1)$/i.test(v)) : [];
  const tempo = num(node.props.Tempo);
  const sw = num(node.props.Swing);
  return {
    kind: "preset", name: node.props.Name || base || "Preset", drums, patterns,
    tempo: tempo >= 1 && tempo <= 999 ? tempo : null, swing: sw >= 0 && sw <= 100 ? sw : 0, stepRate: STEP_RATES[rate] ? rate : "1/16",
    mutes, current: /^[a-l]$/.test(letter) ? letter.charCodeAt(0) - 97 : 0, notes: [],
  };
}

// a Microtonic MIDI file (drag-to-MIDI): the first track with notes. Notes 36-43 = drums 1-8, velocity 100 and up = accent. One pattern of up to 16 steps
// (a step is a sixteenth: ppq / 4 ticks); notes after the first 16 steps are ignored, the tempo is the file's first tempo.
function parseMidi(u8, fileName) {
  const dv = new DataView(u8.buffer, u8.byteOffset, u8.byteLength);
  const u32 = (o) => dv.getUint32(o), u16 = (o) => dv.getUint16(o);
  if (u8.length < 14 || u32(0) !== 0x4d546864) throw fail("That MIDI file is broken.");
  const hl = u32(4), ntrk = u16(10), div = u16(12);
  if (hl < 6 || div & 0x8000 || !div) throw fail("That MIDI file uses a timing Drumlab can't read.");
  let o = 8 + hl, tempoUs = 0;
  const hits = [];
  for (let t = 0; t < Math.min(ntrk, 64) && o + 8 <= u8.length; t++) {
    if (u32(o) !== 0x4d54726b) throw fail("That MIDI file is broken.");
    const end = Math.min(u8.length, o + 8 + u32(o + 4));
    let p = o + 8, tick = 0, run = 0, guard = 0;
    const vlq = () => { let v = 0; for (let i = 0; i < 4 && p < end; i++) { const b = u8[p++]; v = (v << 7) | (b & 127); if (!(b & 128)) break; } return v; };
    while (p < end && guard++ < 200000) {
      tick += vlq();
      let st = u8[p];
      if (st === undefined) break;
      if (st < 0x80) st = run; else { p++; if (st < 0xf0) run = st; }
      if (st === 0xff) { const type = u8[p++], len = vlq(); if (type === 0x51 && len === 3 && !tempoUs) tempoUs = (u8[p] << 16) | (u8[p + 1] << 8) | u8[p + 2]; p += len; if (type === 0x2f) break; }
      else if (st === 0xf0 || st === 0xf7) p += vlq();
      else {
        const kind = st & 0xf0, two = kind !== 0xc0 && kind !== 0xd0;
        const a = u8[p++], b = two ? u8[p++] : 0;
        if (kind === 0x90 && b > 0 && a >= 36 && a <= 43) hits.push({ tick, drum: a - 36, vel: b });
      }
    }
    o = end;
  }
  if (!hits.length) throw fail("No Microtonic drum notes (C1 to G1) in that MIDI file.");
  const per = div / 4, ch = [];
  for (let d = 0; d < 8; d++) ch.push(new Uint8Array(16));
  let late = 0;
  for (const h of hits) { const s = Math.round(h.tick / per); if (s >= 16) { late++; continue; } ch[h.drum][s] |= 1 | (h.vel >= 100 ? 2 : 0); }
  const tempo = tempoUs ? Math.round(6e7 / tempoUs) : null;
  const name = String(fileName || "pattern").replace(/^.*[\\/]/, "").replace(/\.[^.]*$/, "");
  return { kind: "midi", name, drums: [], patterns: [{ steps: 16, chained: false, ch }], tempo: tempo >= 1 && tempo <= 999 ? tempo : null, swing: 0, stepRate: "1/16", mutes: [], current: 0, notes: late ? [`${late} notes after step 16 were left out.`] : [] };
}

// ======================================================================= into a project
// Drums into sounds. targets: sound numbers 0..15 (one per drum, in order). side: 0 = patch A, 1 = patch B.
// The sounds are set to play that side (knobs: no pitch change, morph all the way to A or B), so what you import is what you hear.
export function putDrums(P, drums, targets, side) {
  drums.forEach((d, i) => {
    const s = targets[i];
    if (s == null || s < 0 || s >= NS) return;
    P.patches[s][side] = Uint16Array.from(d.p);
    P.knob[s] = [PITCH_MID, side ? 255 : 0];
  });
}

// Which drums should share a lane? The pairing of the 8 drums into 4 pairs that makes the fewest hits collide (hits on the same step, both unmuted),
// over the patterns that come along. Returns ord: drum d -> place 0-7 (place % 4 is the lane). The identity (1+5, 2+6, ...) stays unless another pairing is better.
export function fitLanes(preset, which) {
  const w = Array.from({ length: 8 }, () => new Array(8).fill(0));
  preset.patterns.forEach((pt, i) => {
    if (which !== "all" && which !== i) return;
    for (let st = 0; st < 16; st++) for (let a = 0; a < 8; a++) for (let b = a + 1; b < 8; b++) {
      const f = st % pt.steps;
      if (pt.ch[a][f] & pt.ch[b][f] & 1 && !preset.mutes[a] && !preset.mutes[b]) w[a][b]++;
    }
  });
  const cost = (pairs) => pairs.reduce((n, [a, b]) => n + w[a][b], 0);
  let best = [[0, 4], [1, 5], [2, 6], [3, 7]], bestCost = cost(best);
  const walk = (left, acc) => {
    if (!left.length) { const c = cost(acc); if (c < bestCost) { bestCost = c; best = acc.map((p) => p.slice()); } return; }
    const a = left[0];
    for (let k = 1; k < left.length; k++) walk(left.filter((_, i) => i && i !== k), [...acc, [a, left[k]]]);
  };
  walk([0, 1, 2, 3, 4, 5, 6, 7], []);
  const ord = new Array(8).fill(0);
  best.sort((x, y) => x[0] - y[0]).forEach(([a, b], lane) => { ord[a] = lane; ord[b] = lane + 4; });
  return ord;
}
export const isIdentity = (ord) => !ord || ord.every((v, i) => v === i);

// Microtonic patterns -> Drumlab patterns.
//   preset   a parsed file, opts: { half: 0|1 (drums land on sounds 1-8 or 9-16), which: "all" | 0..11 (one pattern), first: 0..15 (where the first one lands),
//            extras: set tempo, swing, the chain and the open pattern too, order: from fitLanes (drum d on place order[d]; default d) }
// The rules (one place, and the UI quotes them):
//   - drum d (1-8) is sound 8 * half + d; its lane is d % 4 (so drums 1 and 5 share a lane), its group (the trig byte's bits 4-5) is the rest.
//   - one hit per lane per step: when drum d and d + 4 both hit, drum d (1-4) stays: Microtonic's own PO-32 transfer lets channels 1-4 cut 5-8.
//     (With "fit lanes" the drums are paired differently, whichever pairing loses the fewest hits, and the kit is placed the same way.)
//   - an accent is the step's (as on the PO-32): the step is accented when any hit that stays on it was.
//   - muted drums are left out. Fills (rolls) are left out. A pattern shorter than 16 steps is repeated to fill 16.
//   - hits take the sound's knobs as the pads do (pitch, morph), so the pattern plays the patch side that was imported.
//   - tempo: Microtonic's step rate is its own: Drumlab's step is a sixteenth, so the tempo is scaled (1/8 = half ... 1/32 = double) and kept within 60-240.
//   - swing: 0-100 % -> 0-12.
// Returns { placed: [dest pattern numbers], dropped (hits), muted (hits), fills (steps), spread (steps whose accent reaches a hit that had none), notes: [] }.
export function mapPatterns(P, preset, opts) {
  const half = opts.half ? 1 : 0, extras = !!opts.extras;
  const src = [];
  preset.patterns.forEach((pt, i) => { if (opts.which === "all" || opts.which === i) src.push(i); });
  const rep = { placed: [], dropped: 0, muted: 0, fills: 0, spread: 0, notes: [], hits: 0 };
  if (!src.length) return rep;
  const first = opts.first || 0;
  const ord = opts.order || [0, 1, 2, 3, 4, 5, 6, 7];                     // drum d sits at place ord[d] of the 8 sounds (0-7); by default drum d at place d
  const byLane = [[], [], [], []];
  for (let d = 0; d < 8; d++) byLane[ord[d] & 3].push(d);                  // ascending: the lower drum wins a shared lane
  const groupOf = (d) => (ord[d] >> 2) + 2 * half;
  src.forEach((si, k) => {
    const to = first + k;
    if (to >= NPAT) { rep.notes.push(`Patterns after ${NPAT} don't fit and were left out.`); return; }
    const pt = preset.patterns[si], q = newPattern();
    for (let st = 0; st < 16; st++) {
      const from = st % pt.steps;
      let acc = 0, anyPlain = 0;
      for (let lane = 0; lane < LANES; lane++) {
        let pick = -1, accent = 0;
        for (const d of byLane[lane]) {                          // the lower drum first: 1-4 win over 5-8
          const v = pt.ch[d] ? pt.ch[d][from] : 0;
          if (v & 4) rep.fills++;
          if (!(v & 1)) continue;
          if (preset.mutes[d]) { rep.muted++; continue; }
          if (pick >= 0) { rep.dropped++; continue; }
          pick = d; accent = v & 2;
        }
        if (pick < 0) continue;
        const s = 8 * half + ord[pick], i = lane * 16 + st;
        q.trig[i] = 1 | (groupOf(pick) << 4);
        q.lock[i * 2] = P.knob[s][0]; q.lock[i * 2 + 1] = P.knob[s][1];
        rep.hits++;
        if (accent) acc = 1; else anyPlain = 1;
      }
      if (acc) { q.acc |= 1 << st; rep.spread += anyPlain; }
    }
    P.pats[to] = q; rep.placed.push(to);
  });
  if (extras) {
    if (preset.tempo != null) {
      const spq = STEP_RATES[preset.stepRate] || 4, want = (preset.tempo * spq) / 4, b = clampBpm(want);
      P.bpm = b;
      if (b !== Math.round(want)) rep.notes.push(`Tempo ${Math.round(want)} is outside 60-240, set to ${b}.`);
    }
    P.swing = Math.max(0, Math.min(12, Math.round((preset.swing * 12) / 100)));
    if (opts.which === "all") {
      const run = [];
      let c = preset.current;
      while (c < preset.patterns.length && run.length < CHAIN_MAX) { run.push(first + c); if (!preset.patterns[c].chained) break; c++; }
      if (run.every((v) => v < NPAT) && first + preset.current < NPAT) {
        P.curPat = first + preset.current;
        P.chainN = run.length > 1 ? run.length : 0;
        P.chain = run.concat(new Array(CHAIN_MAX - run.length).fill(0));
      }
    } else P.curPat = rep.placed[0];
  }
  return rep;
}

// ======================================================================= out: 21 u16 -> text
// Number as text with 10 decimals trimmed: round trips through toU16 exactly.
const fx = (v, d = 8) => (v >= 0 ? "+" : "") + v.toFixed(d);
const fp = (v, d = 8) => v.toFixed(d);
const mode3 = (v, names) => names[v < 1 / 3 ? 0 : v < 2 / 3 ? 1 : 2];
export function drumLines(p, name) {
  const x = Array.from(p, (v) => v / 65535);
  const mm = x[4] < 1 / 3 ? 0 : x[4] < 2 / 3 ? 1 : 2, em = x[10] < 1 / 3 ? 0 : x[10] < 2 / 3 ? 1 : 2;
  const atk = (v) => attackSeconds(v) * 1000, lin = em === 1 ? 1 / 1.5 : 1;
  const rate = mm === 0 ? (x[5] <= 0 ? "inf ms" : fp(((1.03 - x[5]) / (3 * x[5])) * 1000) + " ms") : fp(modHz(x[5], mm === 1)) + " Hz";
  const b = 2 * x[6] - 1, sm = b * Math.abs(b) * (mm === 0 ? 96 : 48);
  const lv = x[17] <= 0 ? "-inf dB" : fx(60 * x[17] - 49 - 1 / x[17]) + " dB";
  return [
    `Name: "${String(name || "Drumlab").replace(/["\\\n\r]/g, " ").slice(0, 40)}"`,
    `OscWave: ${mode3(x[0], ["Sine", "Triangle", "Saw"])}`,
    `OscFreq: ${fp(20 * Math.pow(1000, x[1]))} Hz`,
    `OscAtk: ${fp(atk(x[2]))} ms`,
    `OscDcy: ${fp(1000 * Math.pow(10, 3 * x[3] - 2))} ms`,
    `ModMode: ${["Decay", "Sine", "Noise"][mm]}`,
    `ModRate: ${rate}`,
    `ModAmt: ${fx(sm)} sm`,
    `NFilMod: ${mode3(x[7], ["LP", "BP", "HP"])}`,
    `NFilFrq: ${fp(20 * Math.pow(1000, x[8]))} Hz`,
    `NFilQ: ${fp(0.1 * Math.pow(10, 5 * x[9]))}`,
    `NStereo: Off`,
    `NEnvMod: ${["Exp", "Lin", "Mod"][em]}`,
    `NEnvAtk: ${fp(atk(x[11]) * lin)} ms`,
    `NEnvDcy: ${fp(1000 * Math.pow(10, 3 * x[12] - 2) * lin)} ms`,
    `Mix: ${fp((1 - x[13]) * 100)} / ${fp(x[13] * 100)}`,
    `DistAmt: ${fp(x[14] * 100)}`,
    `EQFreq: ${fp(20 * Math.pow(1000, x[15]))} Hz`,
    `EQGain: ${fx(x[16] * 80 - 40)} dB`,
    `Level: ${lv}`,
    `Pan: ${fx(0)}`,
    `Output: A`,
    `Choke: Off`,
    `OscVel: ${fp(x[18] * 200)}%`,
    `NVel: ${fp(x[19] * 200)}%`,
    `ModVel: ${fp(x[20] * 200)}%`,
  ];
}
// a .mtdrum
export function writeDrum(p, name) { return "MicrotonicDrumPatchV3: {\n" + drumLines(p, name).map((l) => "\t" + l).join("\n") + "\n}\n"; }

const bar = (bits, ch) => { let s = ""; for (let i = 0; i < 16; i++) s += bits[i] ? ch : "-"; return s; };
// a .mtpreset from 8 sounds: drums = [{ p, name }] (up to 8), patterns = [{ hits: [8 x Uint8Array(16) of 1 | 2 accent] }] (up to 12; missing ones are empty)
export function writePreset({ name, drums, patterns, tempo, swing100, current }) {
  const L = [];
  L.push("MicrotonicPresetV3: {");
  L.push(`\tName: "${String(name || "Drumlab").replace(/["\\\n\r]/g, " ").slice(0, 60)}"`, "\tModified: false", `\tTempo: ${fp(tempo || 120)} bpm`, `\tPattern: ${String.fromCharCode(97 + (current || 0))}`,
    "\tStepRate: 1/16", `\tSwing: ${fp(swing100 || 0)}%`, "\tFillRate: 2.00000000x", "\tMastVol: 0.00000000 dB", "\tMutes: { Off, Off, Off, Off, Off, Off, Off, Off }", "\tDrumPatches: {");
  for (let d = 0; d < 8; d++) {
    const dr = drums[d] || { p: new Uint16Array(NP), name: `Empty ${d + 1}` };
    L.push(`\t\t${d + 1}: {`, ...drumLines(dr.p, dr.name).map((l) => "\t\t\t" + l), "\t\t}");
  }
  L.push("\t}", "\tPatterns: {");
  for (let i = 0; i < 12; i++) {
    const pt = patterns[i], ch = pt ? pt.hits : [];
    L.push(`\t\t${String.fromCharCode(97 + i)}: {`, "\t\t\tLength: 16", ...(i < 11 ? [`\t\t\tChained: ${pt && pt.chained ? "true" : "false"}`] : []));
    for (let d = 0; d < 8; d++) {
      const h = ch[d] || new Uint8Array(16);
      L.push(`\t\t\t${d + 1}: {`, `\t\t\t\tTriggers: ${bar(h.map((v) => v & 1), "#")}`, `\t\t\t\tAccents:  ${bar(h.map((v) => v & 2), "#")}`, "\t\t\t\tFills:    ----------------", "\t\t\t}");
    }
    L.push("\t\t}");
  }
  L.push("\t}", "}", "");
  return L.join("\n");
}

// A project's 8 sounds (half 0 = sounds 1-8, 1 = 9-16, side 0/1 = patch A/B) and its patterns (first .. first + 11) as a preset's text.
export function presetFromProject(P, { half, side, first, name, patterns = true }) {
  const drums = [], pats = [];
  for (let d = 0; d < 8; d++) { const s = 8 * half + d; drums.push({ p: P.patches[s][side], name: `${name} ${s + 1}${side ? "B" : "A"}` }); }
  if (patterns) {
    for (let i = 0; i < 12; i++) {
      const n = (first || 0) + i;
      if (n >= NPAT) break;
      const q = P.pats[n], hits = [];
      for (let d = 0; d < 8; d++) {
        const s = 8 * half + d, lane = s & 3, grp = s >> 2, row = new Uint8Array(16);
        for (let st = 0; st < 16; st++) {
          const t = q.trig[lane * 16 + st];
          if ((t & 15) && ((t >> 4) & 3) === grp) row[st] = 1 | ((t & 0x80) || (q.acc >> st) & 1 ? 2 : 0);
        }
        hits.push(row);
      }
      pats.push({ hits });
    }
  }
  return writePreset({ name: `${name} ${half ? "9-16" : "1-8"}${side ? "B" : "A"}`, drums, patterns: pats, tempo: P.bpm, swing100: (P.swing * 100) / 12, current: 0 });
}
