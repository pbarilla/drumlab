// SPDX-License-Identifier: GPL-3.0-only
// The Microtonic import / export and the share files (site/js/mtpreset.js, drumproj.js, imports.js, zip.js).
//   node tests/test_mtimport.mjs            (LIBPO=<libpo>/src/core adds the libpo checks; the folders in tests/mtlib.mjs add the real-file checks)
import { readFileSync, writeFileSync, mkdirSync, readdirSync } from "node:fs";
import { execFileSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { parseMicrotonic, writeDrum, writePreset, drumFromProps, toU16, mapPatterns, putDrums, fitLanes, isIdentity, MAX_BYTES } from "../site/js/mtpreset.js";
import * as D from "../site/js/drumproj.js";
import { readItem, defaultPlan, applyItem, describe, exportPresets, exportDrums, exportKit, exportPatterns, zipFiles } from "../site/js/imports.js";
import { crc32 } from "../site/js/zip.js";
import { SAMPLE_DIRS, EXTRA_DIRS, walk, sample, rng, drumTexts, libpoParse, read } from "./mtlib.mjs";

const root = fileURLToPath(new URL("..", import.meta.url));
let failed = 0;
const ok = (c, w) => { console.log(`${w.padEnd(78)} ${c ? "ok" : "FAIL"}`); if (!c) failed++; };
const enc = (s) => new TextEncoder().encode(s);
const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
const sample1 = (n) => new Uint8Array(readFileSync(root + "site/projects/samples/" + n));
const throwsBad = (fn) => { try { fn(); return false; } catch (e) { return e && e.code === "badfile"; } };
const R = rng(42);
const ri = (n) => Math.floor(R() * n);

// ---------------------------------------------------------------- one drum in four spellings
const D3 = `MicrotonicDrumPatchV3: {
\tName: "Test BD"
\tOscWave: Triangle
\tOscFreq: 289.8004138907966 Hz
\tOscAtk: 12.5 ms
\tOscDcy: 42.73351893705825 ms
\tModMode: Decay
\tModRate: 683.9503298299351 ms
\tModAmt: +56.918397180175816 sm
\tNFilMod: HP
\tNFilFrq: 1533.2046135988762 Hz
\tNFilQ: 5.297768379219882
\tNStereo: Off
\tNEnvMod: Lin
\tNEnvAtk: 3.5 ms
\tNEnvDcy: 275.4171232289388 ms
\tMix: 60.0 / 40.0
\tDistAmt: 40.25000035762787
\tEQFreq: 775.2255799675168 Hz
\tEQGain: -6.0 dB
\tLevel: -4.99565734070962 dB
\tPan: 0.0
\tOscVel: 50.0%
\tNVel: 20.0%
\tModVel: 100.0%
}
`;
const eq = (text) => parseMicrotonic(enc(text), "x.mtdrum").drums[0].p;
const ref = eq(D3);
ok(ref.length === 21 && ref[0] === 32768 && ref[4] === 0 && ref[7] === 65535 && ref[10] === 32768 && ref[13] === toU16(0.4), "V3 colon spelling: the discrete values and the Mix");
const asEq = D3.replace(/^MicrotonicDrumPatchV3: \{/, "MicroTonicPresetV3: {\n\tDrumPatches: {\n\t\t1: {").replace(/\n}\n$/, "\n\t\t}\n\t}\n}\n")
  .replace(/: /g, " = ").replace(/ (Hz|ms|sm|dB|%)\n/g, "\n").replace(/Mix = 60.0 \/ 40.0/, "Mix = 60.0").replace("DrumPatches = {", "DrumPatches: {").replace("1 = {", "1: {").replace("MicroTonicPresetV3 = {", "MicroTonicPresetV3: {");
ok(same(parseMicrotonic(enc(asEq), "x.mtpreset").drums[0].p, ref), 'the same drum as a V3 preset, "=" and no units (Patternarium style)');
const v1 = "MicroTonicDrumPatchV1={\r\n" + D3.split("\n").slice(1, -2).map((l) => "\t" + l.trim().replace(": ", " = ")).join("\r\n") + "\r\n}\r\n";
ok(same(eq(v1), ref), "the same drum as a V1 .mtdp (CRLF)");
const flat = D3.split("\n").slice(1, -2).map((l) => l.trim().replace(/ Hz| ms| sm| dB/g, (m) => m)).join("\n").replace("Mix: 60.0 / 40.0", "Mix: 60.000% osc, 40.000% noise") + "\n";
ok(same(eq(flat), ref), "the same drum as bare lines (libpo's own sample style)");
ok(parseMicrotonic(enc(D3), "x").kind === "drum" && parseMicrotonic(enc(D3), "x").name === "Test BD", "a drum file: kind and name");

// libpo on the same text, when we can
const L = process.env.LIBPO;
let refBin = null;
if (L) {
  try {
    mkdirSync(root + "build/host", { recursive: true });
    refBin = root + "build/host/ref_import";
    execFileSync("cc", ["-O2", `-I${L}/include`, `-I${L}/src`, "-o", refBin, root + "tests/drum/ref_import.c", `${L}/src/po32.c`, `${L}/src/po32_patch_import.c`], { stdio: "pipe" });
  } catch (_) { refBin = null; }
}
if (refBin) {
  const theirs = libpoParse([D3.split("\n").slice(1, -2).join("\n") + "\n"], refBin)[0];
  ok(theirs && ref.every((v, i) => Math.abs(v - theirs[i]) <= 1), "the fixture drum: every one of 21 values within 1 of libpo's C");
} else console.log("(libpo checks skipped: set LIBPO to its src/core folder)");

// ---------------------------------------------------------------- export and read back: 21 u16 must survive
let rt = 0, rtBad = 0;
for (let n = 0; n < 400; n++) {
  const p = new Uint16Array(21);
  for (let i = 0; i < 21; i++) p[i] = R() < 0.1 ? [0, 65535][ri(2)] : ri(65536);
  for (const i of [0, 4, 7, 10]) p[i] = [0, 32768, 65535][ri(3)];
  if (p[17] > 0 && p[17] < 150) p[17] = 150;                               // libpo (and so we) read levels under -500 dB as silence
  rt++;
  const back = parseMicrotonic(enc(writeDrum(p, "rt")), "rt.mtdrum").drums[0].p;
  if (!same(back, p)) { rtBad++; if (rtBad < 4) console.log("  differs:", Array.from(p).join(","), "->", Array.from(back).join(",")); }
}
ok(rtBad === 0, `export a patch as .mtdrum and read it back: ${rt} random patches, all 21 values equal`);

// ---------------------------------------------------------------- a preset, and the pattern rules
const drumBlock = (i) => D3.split("\n").slice(1, -2).map((l) => "\t\t\t" + l.trim()).join("\n").replace('Name: "Test BD"', `Name: "Drum ${i}"`);
const pat = (name, rows, len = 16, chained = false) => `\t\t${name}: {\n\t\t\tLength: ${len}\n\t\t\tChained: ${chained}\n` + rows.map((r, d) => `\t\t\t${d + 1}: {\n\t\t\t\tTriggers: ${r[0] || "-".repeat(16)}\n\t\t\t\tAccents:  ${r[1] || "-".repeat(16)}\n\t\t\t\tFills:    ${r[2] || "-".repeat(16)}\n\t\t\t}`).join("\n") + "\n\t\t}\n";
const emptyRows = () => new Array(8).fill(0).map(() => []);
const rowsA = emptyRows();
rowsA[0] = ["#---#---#---#---", "#---------------"];                         // drum 1 on 0 4 8 12, accent on step 0
rowsA[4] = ["#-------#-------"];                                              // drum 5 (same lane): collides at step 0, alone at step 8 (drum 1 hits there too!)
rowsA[1] = ["----#-------#---", "----#-----------", "----#-----------"];       // drum 2, accent + fill on 4
rowsA[2] = ["--#-------------"];                                              // drum 3 (muted below): three hits... one
rowsA[7] = ["---------#------"];                                              // drum 8, lane 3, group 1
const rowsB = emptyRows(); rowsB[0] = ["#-#-"];
const PRE = (mutes, tempo, rate) => `MicrotonicPresetV3: {\n\tName: "Fixture"\n\tModified: false\n\tTempo: ${tempo} bpm\n\tPattern: b\n\tStepRate: ${rate}\n\tSwing: 50.0%\n\tFillRate: 2.0x\n\tMastVol: 0.0 dB\n\tMutes: { ${mutes.join(", ")} }\n\tDrumPatches: {\n` + new Array(8).fill(0).map((_, i) => `\t\t${i + 1}: {\n${drumBlock(i + 1)}\n\t\t}\n`).join("") + "\t}\n\tPatterns: {\n" + pat("a", rowsA, 16, false) + pat("b", rowsB, 4, true) + pat("c", emptyRows(), 16, false) + "\t}\n}\n";
const OFF = ["Off", "Off", "On", "Off", "Off", "Off", "Off", "Off"];
const pre = parseMicrotonic(enc(PRE(OFF, 124, "1/16")), "fx.mtpreset");
ok(pre.kind === "preset" && pre.drums.length === 8 && pre.patterns.length === 3 && pre.tempo === 124 && pre.swing === 50 && pre.mutes[2] && pre.current === 1 && pre.patterns[1].chained && pre.patterns[1].steps === 4, "fixture preset: 8 drums, 3 patterns, tempo, swing, mutes, chain flag");
ok(pre.drums.every((d) => same(d.p, ref)), "fixture preset: every drum equals the single drum");
const P0 = D.decodeProject(sample1("two-step.drumlab"));
const P1 = D.cloneProject(P0);
const rep = mapPatterns(P1, pre, { half: 1, which: "all", first: 2, extras: true });
const q = P1.pats[2];
ok(rep.placed.join() === "2,3,4", "patterns land from pattern 3 on");
ok(q.trig[0 * 16 + 0] === (1 | (2 << 4)) && q.trig[0 * 16 + 8] === (1 | (2 << 4)) && q.trig[0 * 16 + 4] === (1 | (2 << 4)), "drum 1 -> sound 9 (lane 1, group 2); at step 8 drum 1 wins over drum 5");
ok(rep.dropped === 2, "hits dropped where drums 1 and 5 both hit (steps 0 and 8): 2");
ok(q.trig[1 * 16 + 4] === (1 | (2 << 4)) && q.trig[1 * 16 + 12] === (1 | (2 << 4)), "drum 2 on lane 2");
ok(q.trig[3 * 16 + 9] === (1 | (3 << 4)), "drum 8 -> lane 4, group 3 (sound 16)");
ok((q.acc & (1 << 0)) && (q.acc & (1 << 4)) && !(q.acc & (1 << 8)), "accents are per step: steps 1 and 5 lifted");
ok(q.trig[2 * 16 + 2] === 0 && rep.muted === 1, "the muted drum 3 leaves no hit (1 counted)");
ok(rep.fills === 1, "fills are counted (and left out)");
ok(P1.pats[3].trig[0] === 33 && P1.pats[3].trig[2] === 33 && P1.pats[3].trig[14] === 33 && D.hitCount(P1.pats[3]) === 8 && P1.pats[3].trig[1] === 0, "a 4 step pattern (# - # -) is repeated to 16 steps: 8 hits");
ok(P1.bpm === 124 && P1.swing === 6 && P1.curPat === 3 && P1.chainN === 2 && P1.chain[0] === 3 && P1.chain[1] === 4, "tempo 124, swing 50 % -> 6, open pattern B (4), B is chained to C: chain 4, 5");
ok(q.lock[0] === P1.knob[8][0] && q.lock[1] === P1.knob[8][1], "hits take the knobs of the sound (as the pads do)");
const slow = D.cloneProject(P0); mapPatterns(slow, parseMicrotonic(enc(PRE(OFF, 120, "1/8")), "x"), { half: 0, which: 0, first: 0, extras: true });
ok(slow.bpm === 60, "step rate 1/8 at 120 -> 60 bpm");
const fast = D.cloneProject(P0); const rf = mapPatterns(fast, parseMicrotonic(enc(PRE(OFF, 200, "1/32")), "x"), { half: 0, which: 0, first: 0, extras: true });
ok(fast.bpm === 240 && rf.notes.length === 1, "step rate 1/32 at 200 -> 400, clamped to 240, said so");
const trip = D.cloneProject(P0); mapPatterns(trip, parseMicrotonic(enc(PRE(OFF, 100, "1/16T")), "x"), { half: 0, which: 0, first: 0, extras: true });
ok(trip.bpm === 150, "step rate 1/16T at 100 -> 150");

// fit lanes: a pairing that loses fewer hits, and the kit goes where the patterns expect it
const ord = fitLanes(pre, "all");
ok(!isIdentity(ord) && new Set(ord.map((v) => v & 3)).size === 4 && ord.slice().sort().join() === "0,1,2,3,4,5,6,7", "fit lanes: a different pairing, a valid one (4 lanes, each with 2 drums)");
const PF = D.cloneProject(P0), repF = mapPatterns(PF, pre, { half: 0, which: "all", first: 0, extras: false, order: ord });
ok(repF.dropped === 0 && rep.dropped === 2, "fit lanes: fewer hits dropped (" + repF.dropped + " instead of " + rep.dropped + ")");
const itF = readItem("fx.mtpreset", enc(PRE(OFF, 124, "1/16"))), plF = defaultPlan(itF), PK = D.cloneProject(P0);
const lines = applyItem(PK, itF, plF);
ok(plF.fit === true && lines.some((l) => /moved/.test(l)) && same(PK.patches[ord[4]][0], pre.drums[4].p), "fit lanes: on by default, the kit lands on the fitted sounds, and the page says which drums moved");
let hitOk = true;                                    // every hit of the pattern plays the sound of its drum: group and lane agree with the place
for (let d = 0; d < 8; d++) { const place = ord[d]; for (let st = 0; st < 16; st++) { const v = pre.patterns[0].ch[d][st % pre.patterns[0].steps]; if ((v & 1) && !pre.mutes[d]) { const t = PF.pats[0].trig[(place & 3) * 16 + st]; if (!(t & 15) || ((t >> 4) & 3) !== (place >> 2)) hitOk = false; } } }
ok(hitOk, "fit lanes: every unmuted hit is on the lane and group of its drum's new sound");

// the chain: the open pattern and the chained flags
const chainSrc = PRE(OFF, 120, "1/16").replace("Pattern: b", "Pattern: a").replace(/(\t\ta: \{\n\t\t\tLength: 16\n\t\t\tChained: )false/, "$1true");
const pc = D.cloneProject(P0); mapPatterns(pc, parseMicrotonic(enc(chainSrc), "x"), { half: 0, which: "all", first: 1, extras: true });
ok(pc.chainN === 3 && pc.chain.slice(0, 3).join() === "1,2,3" && pc.curPat === 1, "chained flags A and B (open pattern A, first = 2): chain 2, 3, 4");

// drums into sounds
const P2 = D.cloneProject(P0);
putDrums(P2, pre.drums, pre.drums.map((_, i) => 8 + i), 1);
ok(same(P2.patches[8][1], ref) && same(P2.patches[15][1], ref) && same(P2.patches[8][0], P0.patches[8][0]) && P2.knob[10][1] === 255 && P2.knob[10][0] === 0x80, "kit to sounds 9-16, patch B: A untouched, the sounds play B");

// ---------------------------------------------------------------- Microtonic export: files Microtonic can read, and we can read back
const E = exportPresets(P0, "proj", 0);
ok(E.length === 4 && E[0].name === "proj-sounds-1-8-A.mtpreset" && E[3].name === "proj-sounds-9-16-B.mtpreset", "export: 4 presets, named by sounds and patch");
const eb = parseMicrotonic(enc(E[2].data), E[2].name);
ok(eb.kind === "preset" && eb.drums.length === 8 && eb.patterns.length === 12 && same(eb.drums[0].p, P0.patches[8][0]) && same(eb.drums[7].p, P0.patches[15][0]), "a preset we wrote reads back: drums are sounds 9-16, patch A");
ok(eb.tempo === P0.bpm && Math.abs(eb.swing - (P0.swing * 100) / 12) < 1e-6, "tempo and swing come back");
const back = D.cloneProject(P0);
for (let n = 0; n < 12; n++) back.pats[n] = D.newPattern();
mapPatterns(back, eb, { half: 1, which: "all", first: 0, extras: false });
let patOk = true;
for (let n = 0; n < 12; n++) for (let l = 0; l < 4; l++) for (let s = 0; s < 16; s++) {
  const t = P0.pats[n].trig[l * 16 + s], hit = (t & 15) && ((t >> 4) & 3) >= 2;
  const u = back.pats[n].trig[l * 16 + s];
  if (hit ? u !== t : u !== 0) patOk = false;
  if (hit && ((P0.pats[n].acc >> s) & 1) !== ((back.pats[n].acc >> s) & 1)) patOk = false;
}
ok(patOk, "patterns 1-12 of sounds 9-16 out and back in: the same hits and accents");
const dr = exportDrums(P0, "proj");
ok(dr.length === 32 && dr[0].name === "proj-sound-01-A.mtdrum" && dr[31].name === "proj-sound-16-B.mtdrum" && same(parseMicrotonic(enc(dr[5].data), "x").drums[0].p, P0.patches[2][1]), "export: 32 .mtdrum, one per sound and patch, and they read back");
// zip: the system's unzip must accept it
const z = zipFiles(E);
ok(z[0] === 0x50 && z[1] === 0x4b && z.length > 1000 && crc32(enc("123456789")) === 0xcbf43926, "zip: a zip header, crc32 of '123456789' is cbf43926");
try { mkdirSync(root + "build/host", { recursive: true }); writeFileSync(root + "build/host/t.zip", z); const o = execFileSync("unzip", ["-t", root + "build/host/t.zip"], { stdio: "pipe" }).toString(); ok(/No errors/.test(o) && o.includes("proj-sounds-1-8-A.mtpreset"), "zip: `unzip -t` says no errors, four files"); } catch (e) { console.log("(unzip not available, zip check skipped)"); }

// ---------------------------------------------------------------- the share files
const randProject = () => {
  const P = D.decodeProject(sample1("breakbeat.drumlab"));
  for (let s = 0; s < 16; s++) { for (let q2 = 0; q2 < 2; q2++) for (let i = 0; i < 21; i++) P.patches[s][q2][i] = ri(65536); P.knob[s] = [ri(256), ri(256)]; P.names[s] = ri(33); }
  for (let n = 0; n < 16; n++) { const p = D.newPattern(); for (let i = 0; i < 64; i++) { if (R() < 0.25) p.trig[i] = 1 + ri(15) | ri(4) << 4 | (R() < 0.1 ? 128 : 0); p.lock[i * 2] = ri(256); p.lock[i * 2 + 1] = ri(256); } p.acc = ri(65536); for (let i = 0; i < 16; i++) p.fx[i] = ri(65536); P.pats[n] = p; }
  P.bpm = 60 + ri(181); P.swing = ri(13); P.chainN = ri(10); for (let i = 0; i < 64; i++) P.chain[i] = i < P.chainN ? ri(16) : 0;
  return P;
};
let shareOk = true, shareN = 0;
for (let n = 0; n < 20; n++) {
  const A = randProject(), B = randProject();
  const kit = D.readKitFile(D.kitFile(A, "k"));
  D.putKit(B, kit);
  const eqK = A.patches.every((two, s) => same(two[0], B.patches[s][0]) && same(two[1], B.patches[s][1]) && same(A.knob[s], B.knob[s])) && same(A.names, B.names);
  const pf = D.readPatternFile(D.patternFile(A, Array.from({ length: 16 }, (_, i) => i), "p"));
  const C = randProject();
  D.putPatterns(C, pf, null, true);
  const eqP = A.pats.every((p, i) => same(p.trig, C.pats[i].trig) && same(p.lock, C.pats[i].lock) && p.acc === C.pats[i].acc && same(p.fx, C.pats[i].fx)) && C.bpm === A.bpm && C.swing === A.swing && C.chainN === A.chainN && same(A.chain.slice(0, A.chainN), C.chain.slice(0, C.chainN));
  // and as bytes: a project made of A's kit and A's patterns is A, byte for byte (the rest of A is its own tempo etc.)
  const M = D.cloneProject(B); D.putKit(M, kit); D.putPatterns(M, pf, null, true); M.curPat = A.curPat;
  const bytesOk = same(D.encodeProject(M), D.encodeProject(A));
  shareN++; if (!(eqK && eqP && bytesOk)) shareOk = false;
}
ok(shareOk, `share files: kit and patterns out and in, ${shareN} random projects, identical to the byte`);
const one = D.readPatternFile(D.patternFile(P0, [5], "one"));
const moved = D.cloneProject(P0); D.putPatterns(moved, one, 9, true);
ok(same(moved.pats[9].trig, P0.pats[5].trig) && moved.bpm === P0.bpm, "a one pattern file lands where you say");
ok(throwsBad(() => D.readKitFile("{}")) && throwsBad(() => D.readKitFile("nope")) && throwsBad(() => D.readPatternFile('{"drumlab-pattern":1,"patterns":[{"n":99}]}')), "share files: junk is refused with a plain error");
ok(throwsBad(() => readItem("a.json", enc('{"drumlab":1,"projects":[]}'))), "a backup file is not an import item (the message says so)");
const itKit = readItem("k.json", enc(D.kitFile(P0, "kk")));
ok(itKit.kind === "kit" && applyItem(D.cloneProject(P0), itKit, defaultPlan(itKit)).length >= 1, "readItem: a kit file");

// ---------------------------------------------------------------- the project codec
for (const n of ["breakbeat", "four-on-the-floor", "half-time", "two-step"]) { const b = sample1(n + ".drumlab"); if (!same(D.encodeProject(D.decodeProject(b)), b)) ok(false, "codec: " + n); }
ok(true, "codec: the 4 sample projects decode and encode to the same bytes");
const v1b = sample1("two-step.drumlab").slice(0, 4549); v1b[0] = 1;
const dv1 = D.decodeProject(v1b), ev1 = D.encodeProject(dv1);
ok(ev1.length === D.SIZE && ev1[0] === 3, "codec: a version 1 project becomes version 3");
ok(throwsBad(() => D.decodeProject(new Uint8Array(100))) && throwsBad(() => D.decodeProject(Uint8Array.from([9, ...new Array(5000).fill(0)]))), "codec: junk is refused");

// ---------------------------------------------------------------- hostile files: never a crash, always a plain error
const real = SAMPLE_DIRS.length ? walk(SAMPLE_DIRS[0]).slice(0, 1) : [];
const seedText = real.length ? read(real[0]) : enc(PRE(OFF, 124, "1/16"));
let hostile = 0, hostileBad = 0;
const tryParse = (u8, why) => {
  hostile++;
  const t0 = Date.now();
  try { parseMicrotonic(u8, "h.mtpreset"); } catch (e) { if (!(e && e.code === "badfile" && typeof e.message === "string" && e.message.length < 90)) { hostileBad++; console.log("  not a plain error:", why, e && e.message); } }
  if (Date.now() - t0 > 3000) { hostileBad++; console.log("  too slow:", why); }
};
for (let i = 0; i < 80; i++) tryParse(seedText.slice(0, Math.floor((seedText.length * i) / 80)), "cut " + i);
for (let i = 0; i < 80; i++) { const c = seedText.slice(); for (let k = 0; k < 1 + ri(40); k++) c[ri(c.length)] = ri(256); tryParse(c, "flipped " + i); }
for (let i = 0; i < 40; i++) { const c = new Uint8Array(1 + ri(3000)); for (let k = 0; k < c.length; k++) c[k] = ri(256); tryParse(c, "random " + i); }
for (let i = 0; i < 20; i++) { const c = new Uint8Array(1 + ri(3000)); for (let k = 0; k < c.length; k++) c[k] = 32 + ri(95); tryParse(c, "text noise " + i); }
tryParse(new Uint8Array(0), "empty"); tryParse(new Uint8Array(MAX_BYTES + 1), "over the limit"); tryParse(enc("{".repeat(100000)), "braces"); tryParse(enc("a: {\n".repeat(50000)), "deep");
tryParse(enc("MicrotonicPresetV3: {\n" + "x: 1\n".repeat(60000) + "}\n"), "many lines"); tryParse(enc("OscFreq: " + "9".repeat(100000) + "\n"), "one huge number"); tryParse(enc("OscFreq: 1e999999\nLevel: -1e999\nModRate: inf\n"), "overflowing numbers");
tryParse(Uint8Array.from([0x4d, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 1, 0, 1, 0, 96, 0x4d, 0x54, 0x72, 0x6b, 0xff, 0xff, 0xff, 0xff, 0x90]), "midi, a lying length");
tryParse(enc("OscFreq: 1\n__proto__: x\nconstructor: {\nprototype: {\n}\n}\n"), "prototype keys");
ok(hostileBad === 0, `hostile files: ${hostile} truncated, flipped, random, huge or deep inputs: a plain error or a result, never a crash`);
ok(({}).polluted === undefined, "hostile files: nothing leaked into Object.prototype");
ok(throwsBad(() => parseMicrotonic(enc("hello world"), "a.txt")) && throwsBad(() => parseMicrotonic(Uint8Array.from([0, 1, 2, 3]), "a.bin")), "a file that isn't Microtonic's: 'That isn't a Microtonic ...'");
// a MIDI pattern (drag to MIDI): one bar, notes 36 and 40, tempo 130
const midi = Uint8Array.from([0x4d, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 0, 0, 1, 0, 96, 0x4d, 0x54, 0x72, 0x6b, 0, 0, 0, 0,
  0, 0xff, 0x51, 3, 0x07, 0x0f, 0x3d, 0, 0x99, 36, 127, 24, 0x89, 36, 0, 0, 0x99, 40, 90, 72, 0x89, 40, 0, 0, 0x99, 36, 100, 24, 0x89, 36, 0, 0, 0xff, 0x2f, 0]);
const len = midi.length - 22; midi[18] = len >> 24; midi[19] = (len >> 16) & 255; midi[20] = (len >> 8) & 255; midi[21] = len & 255;
const mm = parseMicrotonic(midi, "bar.mid");
ok(mm.kind === "midi" && mm.tempo === 130 && mm.patterns[0].ch[0][0] === 3 && mm.patterns[0].ch[4][1] === 1 && mm.patterns[0].ch[0][4] === 3, "a MIDI pattern: tempo 130, drum 1 accented at step 1, drum 5 at 2, drum 1 accented (velocity 100) at 5");

// ---------------------------------------------------------------- real files: everything parses, and what we write passes the firmware's own checks
let cc = true;
try { mkdirSync(root + "build/host/blobs", { recursive: true }); execFileSync("sh", ["-c", `sed -n '/^static int px_ok/,/^}/p' ${root}firmware/src/labio.c > ${root}build/host/px_ok.inc && cc -O1 -g -fsanitize=address,undefined -DCHIPBOX_HOST -DAPP_VER='"dev"' -I${root}firmware/src -I${root}firmware/gen -I${root}build/host -o ${root}build/host/proj_check ${root}tests/proj_check.c -lm`], { stdio: "pipe" }); } catch (e) { cc = false; console.log("(firmware parser check skipped: no C compiler or the host build failed)"); }
let blobN = 0;
const blobs = [];
const stash = (P, tag) => { const f = `${root}build/host/blobs/${tag}-${blobN++}.drumlab`; writeFileSync(f, D.encodeProject(P)); blobs.push(f); };
const items = [["fixture.mtpreset", enc(PRE(OFF, 124, "1/16"))], ["fixture.mtdrum", enc(D3)], ["bar.mid", midi]];
for (const [n, b] of items) { const it = readItem(n, b), P = D.cloneProject(P0); applyItem(P, it, defaultPlan(it)); stash(P, "fx"); }
stash(randProject(), "rand"); stash(randProject(), "rand");
{ const P = D.cloneProject(P0); for (const f of ["fixture.mtdrum"]) { const it = readItem(f, enc(D3)); for (let s = 0; s < 16; s++) applyItem(P, it, { sound: s, side: s & 1 }); } stash(P, "drums"); }
{ const P = D.decodeProject(D.encodeProject(D.decodeProject(v1b))); stash(P, "v1"); }
const dirsAll = [...SAMPLE_DIRS, ...EXTRA_DIRS];
if (dirsAll.length) {
  const files = sample(dirsAll.flatMap((d) => walk(d)), 1500, 3);
  let good = 0, warn = 0, dr = 0, pt = 0, patHits = 0, t0 = Date.now();
  const failures = [];
  for (const f of files) {
    try {
      const it = readItem(f, read(f));
      good++; dr += it.data.drums.length; pt += it.data.patterns.length; warn += it.data.drums.reduce((n, d) => n + d.warn.length, 0);
      if (good % 40 === 0) { const P = D.cloneProject(P0); const pl = defaultPlan(it); if (it.kind === "preset") { pl.half = good % 2; pl.side = (good >> 1) % 2; pl.first = good % 5; } applyItem(P, it, pl); stash(P, "real"); patHits++; }
    } catch (e) { failures.push(f + ": " + e.message); }
  }
  ok(failures.length === 0, `real files: ${files.length} read (${dr} drums, ${pt} patterns, ${warn} unreadable values) in ${Date.now() - t0} ms`);
  for (const f of failures.slice(0, 5)) console.log("  ", f);
  // every preset of the old and new kinds in the small folders, read by the importer in full
  const small = dirsAll.filter((d) => /SonicCharge|web/i.test(d)).flatMap((d) => walk(d));
  let sf = 0; for (const f of small) { try { readItem(f, read(f)); } catch (_) { sf++; } }
  ok(sf === 0, `real files: all ${small.length} files of the smaller sets read`);
} else console.log("(real-file checks skipped: no sample folders)");
if (cc) {
  let outp = "";
  try { outp = execFileSync(root + "build/host/proj_check", blobs, { env: { ...process.env, HOST_FLASH: root + "build/host/blobs.flash" }, stdio: "pipe" }).toString(); } catch (e) { outp = String(e.stdout || "") + String(e.stderr || ""); }
  const lines = outp.trim().split("\n"), fails = lines.filter((l) => !l.startsWith("ok "));
  ok(lines.length === blobs.length && !fails.length && !/runtime error|AddressSanitizer/.test(outp), `firmware: ${blobs.length} generated projects pass px_ok, lab_unpack, lab_pack (same bytes), store and load (ASan, UBSan)`);
  for (const l of fails.slice(0, 5)) console.log("  ", l);
}
if (failed) { console.log(failed + " FAILED"); process.exit(1); }
console.log("microtonic import tests passed");
