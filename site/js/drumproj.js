// SPDX-License-Identifier: GPL-3.0-only
// A Drumlab project as plain JS, and the two small share files.
//
// The project is the firmware's payload (firmware/src/labproj.c, lab_pack), version 1 to 3. We always write version 3 (5077 bytes):
//   5 bytes  version, bpm, swing (0..12), open pattern, chain length          64 bytes  the chain (pattern numbers 0..15)
//   32 bytes the sounds' knobs (pitch, morph) x 16                           1344 bytes 16 sounds x patch A, B x 21 u16 (little endian, PO-32 wire order)
//   16 x 226  the patterns: trig[4][16], lock[4][16][2], acc u16, fx[16] u16  16 bytes  the sounds' names (0 = the kit's own)
//   trig: bits 0-3 fill (0 = no hit), 4-5 sound / 4 (a lane is sound % 4), 7 accent. lock: pitch (0x80 = none), morph (0 = A .. 255 = B).
//
// SHARE FILES (plain JSON, versioned, made to swap and to import again; every number is an integer):
//   kit      {"drumlab-kit": 1, "name": "...", "sounds": [{"a": [21 u16], "b": [21 u16], "knob": [pitch, morph], "name": 0}, ... x 16]}
//   pattern  {"drumlab-pattern": 1, "name": "...", "bpm": 120, "swing": 0, "chain": [0..15, ...],
//             "patterns": [{"n": 0..15, "trig": [[16] x 4], "lock": [[[pitch, morph] x 16] x 4], "acc": u16, "fx": [16 u16]}, ...]}
//   "n" is the pattern's place (0 = pattern 1). bpm, swing and chain are optional. A pattern file holds one to 16 patterns.
// Importing a share file into a project and reading the project back gives the same bytes that were shared.

export const NS = 16, NPAT = 16, CHAIN_MAX = 64, NP = 21, PITCH_MID = 0x80;
const PSZ3 = 64 + 128 + 2 + 32, PSZ1 = 194, PAT0 = 5 + CHAIN_MAX + 2 * NS + 2 * NS * NP * 2;
export const SIZE = PAT0 + NPAT * PSZ3 + NS;                 // 5077
export const MIN_SIZE = PAT0 + NPAT * PSZ1;                  // 4549: the smallest a version 1 project is
const MAX_BYTES = 12 * 1024 - 16;

const fail = (m) => { const e = new Error(m); e.code = "badfile"; return e; };
const clamp = (v, lo, hi) => (v < lo ? lo : v > hi ? hi : v);

export function newPattern() {
  const lock = new Uint8Array(128);
  for (let i = 0; i < 64; i++) { lock[i * 2] = PITCH_MID; lock[i * 2 + 1] = 1; }          // an empty step as the firmware leaves it (step_toggle)
  return { trig: new Uint8Array(64), lock, acc: 0, fx: new Uint16Array(16) };
}

export function decodeProject(u8) {
  if (!(u8 instanceof Uint8Array) || u8.length < MIN_SIZE || u8.length > MAX_BYTES || u8[0] < 1 || u8[0] > 3) throw fail("That isn't a Drumlab project.");
  const ver = u8[0], psz = ver === 1 ? PSZ1 : PSZ3;
  if (u8.length < PAT0 + NPAT * psz) throw fail("That isn't a Drumlab project.");
  const P = { ver, bpm: u8[1], swing: Math.min(u8[2], 12), curPat: u8[3] & 15, chain: [], knob: [], patches: [], pats: [], names: new Uint8Array(NS) };
  P.chainN = Math.min(u8[4], CHAIN_MAX);
  let n = 5;
  for (let i = 0; i < CHAIN_MAX; i++) P.chain.push(u8[n++] & 15);
  for (let s = 0; s < NS; s++) { P.knob.push([u8[n], u8[n + 1]]); n += 2; }
  for (let s = 0; s < NS; s++) {
    const two = [];
    for (let q = 0; q < 2; q++) { const p = new Uint16Array(NP); for (let i = 0; i < NP; i++) { p[i] = u8[n] | (u8[n + 1] << 8); n += 2; } two.push(p); }
    P.patches.push(two);
  }
  for (let p = 0; p < NPAT; p++) {
    const q = newPattern();
    q.trig.set(u8.subarray(n, n + 64)); q.lock.set(u8.subarray(n + 64, n + 192)); q.acc = u8[n + 192] | (u8[n + 193] << 8);
    if (psz === PSZ3) for (let i = 0; i < 16; i++) q.fx[i] = u8[n + 194 + i * 2] | (u8[n + 195 + i * 2] << 8);
    n += psz;
    P.pats.push(q);
  }
  if (ver >= 3 && u8.length >= n + NS) for (let s = 0; s < NS; s++) P.names[s] = u8[n + s] <= 32 ? u8[n + s] : 0;
  return P;
}

export function encodeProject(P) {
  const u8 = new Uint8Array(SIZE);
  u8[0] = 3; u8[1] = P.bpm; u8[2] = P.swing; u8[3] = P.curPat; u8[4] = P.chainN;
  let n = 5;
  for (let i = 0; i < CHAIN_MAX; i++) u8[n++] = P.chain[i] & 15;
  for (let s = 0; s < NS; s++) { u8[n++] = P.knob[s][0]; u8[n++] = P.knob[s][1]; }
  for (let s = 0; s < NS; s++) for (let q = 0; q < 2; q++) for (let i = 0; i < NP; i++) { u8[n++] = P.patches[s][q][i] & 255; u8[n++] = P.patches[s][q][i] >> 8; }
  for (let p = 0; p < NPAT; p++) {
    const q = P.pats[p];
    u8.set(q.trig, n); u8.set(q.lock, n + 64); u8[n + 192] = q.acc & 255; u8[n + 193] = q.acc >> 8;
    for (let i = 0; i < 16; i++) { u8[n + 194 + i * 2] = q.fx[i] & 255; u8[n + 195 + i * 2] = q.fx[i] >> 8; }
    n += PSZ3;
  }
  for (let s = 0; s < NS; s++) u8[n++] = P.names[s];
  return u8;
}

export const cloneProject = (P) => decodeProject(encodeProject(P));

// ---- the kit share file
const ints = (a, max, what) => {
  if (!Array.isArray(a)) throw fail("That kit file has a broken " + what + ".");
  return a.map((v) => { if (!Number.isInteger(v) || v < 0 || v > max) throw fail("That file has a bad number in " + what + "."); return v; });
};
export function kitFile(P, name) {
  return JSON.stringify({
    "drumlab-kit": 1, name: String(name || "Drumlab kit").slice(0, 60),
    sounds: P.patches.map((two, s) => ({ a: Array.from(two[0]), b: Array.from(two[1]), knob: [P.knob[s][0], P.knob[s][1]], name: P.names[s] })),
  }) + "\n";
}
export function readKitFile(text) {
  let o; try { o = JSON.parse(text); } catch (_) { throw fail("That isn't a Drumlab kit file."); }
  if (!o || o["drumlab-kit"] !== 1 || !Array.isArray(o.sounds) || o.sounds.length !== NS) throw fail("That isn't a Drumlab kit file.");
  const sounds = o.sounds.map((s) => {
    const a = ints(s && s.a, 65535, "patch A"), b = ints(s && s.b, 65535, "patch B"), knob = ints(s && s.knob, 255, "knob");
    if (a.length !== NP || b.length !== NP || knob.length !== 2) throw fail("That kit file has a sound of the wrong size.");
    return { a: Uint16Array.from(a), b: Uint16Array.from(b), knob, name: Number.isInteger(s.name) && s.name >= 0 && s.name <= 32 ? s.name : 0 };
  });
  return { kind: "kit", name: String(o.name || "Drumlab kit").slice(0, 60), sounds };
}
// the sounds of a kit file into the project (all 16, both patches, knobs and names)
export function putKit(P, kit) {
  for (let s = 0; s < NS; s++) {
    P.patches[s] = [Uint16Array.from(kit.sounds[s].a), Uint16Array.from(kit.sounds[s].b)];
    P.knob[s] = kit.sounds[s].knob.slice(); P.names[s] = kit.sounds[s].name;
  }
}

// ---- the pattern share file
const rows = (flat, w) => { const r = []; for (let i = 0; i < flat.length; i += w) r.push(Array.from(flat.subarray(i, i + w))); return r; };
export function patternFile(P, which, name) {      // which: an array of pattern numbers 0..15
  const patterns = which.map((n) => {
    const q = P.pats[n], lock = [];
    for (let l = 0; l < 4; l++) { const row = []; for (let s = 0; s < 16; s++) row.push([q.lock[(l * 16 + s) * 2], q.lock[(l * 16 + s) * 2 + 1]]); lock.push(row); }
    return { n, trig: rows(q.trig, 16), lock, acc: q.acc, fx: Array.from(q.fx) };
  });
  return JSON.stringify({ "drumlab-pattern": 1, name: String(name || "Drumlab patterns").slice(0, 60), bpm: P.bpm, swing: P.swing, chain: P.chain.slice(0, P.chainN), patterns }) + "\n";
}
export function readPatternFile(text) {
  let o; try { o = JSON.parse(text); } catch (_) { throw fail("That isn't a Drumlab pattern file."); }
  if (!o || o["drumlab-pattern"] !== 1 || !Array.isArray(o.patterns) || !o.patterns.length || o.patterns.length > NPAT) throw fail("That isn't a Drumlab pattern file.");
  const patterns = o.patterns.map((p) => {
    const q = newPattern();
    if (!p || !Number.isInteger(p.n) || p.n < 0 || p.n >= NPAT || !Array.isArray(p.trig) || p.trig.length !== 4 || !Array.isArray(p.lock) || p.lock.length !== 4) throw fail("That pattern file is broken.");
    for (let l = 0; l < 4; l++) {
      const t = ints(p.trig[l], 255, "trig"); if (t.length !== 16) throw fail("That pattern file is broken.");
      if (!Array.isArray(p.lock[l]) || p.lock[l].length !== 16) throw fail("That pattern file is broken.");
      for (let s = 0; s < 16; s++) {
        const k = ints(p.lock[l][s], 255, "lock"); if (k.length !== 2) throw fail("That pattern file is broken.");
        q.trig[l * 16 + s] = t[s]; q.lock[(l * 16 + s) * 2] = k[0]; q.lock[(l * 16 + s) * 2 + 1] = k[1];
      }
    }
    if (!Number.isInteger(p.acc) || p.acc < 0 || p.acc > 65535) throw fail("That pattern file is broken.");
    q.acc = p.acc;
    const fx = ints(p.fx, 65535, "fx"); if (fx.length !== 16) throw fail("That pattern file is broken.");
    q.fx.set(fx);
    return { n: p.n, pat: q };
  });
  const out = { kind: "patterns", name: String(o.name || "Drumlab patterns").slice(0, 60), patterns };
  if (Number.isInteger(o.bpm) && o.bpm >= 60 && o.bpm <= 240) out.bpm = o.bpm;
  if (Number.isInteger(o.swing) && o.swing >= 0 && o.swing <= 12) out.swing = o.swing;
  if (Array.isArray(o.chain) && o.chain.length <= CHAIN_MAX && o.chain.every((v) => Number.isInteger(v) && v >= 0 && v < NPAT)) out.chain = o.chain.slice();
  return out;
}
// patterns of a pattern file into the project, the first one landing on pattern `first` (0..15) and the rest after it, in file order.
// Without `first` each lands where the file says. Tempo, swing and chain come along only when `extras` is set (the chain only if the patterns land where they were).
export function putPatterns(P, file, first, extras) {
  const base = file.patterns[0].n, shift = first == null ? 0 : first - base;
  const placed = [];
  for (const { n, pat } of file.patterns) {
    const to = n + shift;
    if (to < 0 || to >= NPAT) continue;
    P.pats[to] = { trig: pat.trig.slice(), lock: pat.lock.slice(), acc: pat.acc, fx: pat.fx.slice() };
    placed.push(to);
  }
  if (extras) {
    if (file.bpm != null) P.bpm = file.bpm;
    if (file.swing != null) P.swing = file.swing;
    if (file.chain && shift === 0) { P.chainN = file.chain.length; P.chain = file.chain.concat(new Array(CHAIN_MAX - file.chain.length).fill(0)); }
  }
  return placed;
}

// ---- backups of one project, for quick looks
export const patternIsEmpty = (q) => q.trig.every((t) => !(t & 15));
export const hitCount = (q) => { let n = 0; for (const t of q.trig) if (t & 15) n++; return n; };
export const ranges = (list) => {          // [0,1,2,5] -> "1-3, 6"
  const out = []; let i = 0;
  while (i < list.length) { let j = i; while (j + 1 < list.length && list[j + 1] === list[j] + 1) j++; out.push(j > i ? `${list[i] + 1}-${list[j] + 1}` : `${list[i] + 1}`); i = j + 1; }
  return out.join(", ");
};
export const clampBpm = (b) => clamp(Math.round(b), 60, 240);
