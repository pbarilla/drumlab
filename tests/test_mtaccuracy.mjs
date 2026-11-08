// SPDX-License-Identifier: GPL-3.0-only
// "Does the imported sound sound like the original?" Drums from real Microtonic files: our JavaScript import -> our fixed-point voice (firmware/src/drum.c),
// against libpo's float synth on the same text. Same statistics as CLAUDE.md "Accuracy against libpo's float synth".
//   LIBPO=<libpo>/src/core node tests/test_mtaccuracy.mjs [drums, default 200]
import { execFileSync } from "node:child_process";
import { mkdirSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { parseMicrotonic } from "../site/js/mtpreset.js";
import { SAMPLE_DIRS, EXTRA_DIRS, walk, sample, rng, drumTexts, libpoParse, read } from "./mtlib.mjs";

const root = fileURLToPath(new URL("..", import.meta.url));
if (!process.env.LIBPO) { console.log("test_mtaccuracy: LIBPO not set, skipped"); process.exit(0); }
const dirs = [...SAMPLE_DIRS, ...EXTRA_DIRS];
if (!dirs.length) { console.log("test_mtaccuracy: no sample folders, skipped"); process.exit(0); }
const L = process.env.LIBPO, bin = root + "build/host/imp_cmp", want = Number(process.argv[2]) || 200;
mkdirSync(root + "build/host", { recursive: true });
execFileSync("cc", ["-O2", `-I${L}/include`, `-I${L}/src`, `-I${root}firmware/src`, `-I${root}firmware/gen`, "-o", bin, `${L}/src/po32.c`, `${L}/src/po32_synth.c`, `${L}/src/po32_patch_import.c`, root + "firmware/src/drum.c", root + "tests/drum/imp_cmp.c", "-lm"], { stdio: "inherit" });

execFileSync("cc", ["-O2", `-I${L}/include`, `-I${L}/src`, "-o", root + "build/host/ref_import", root + "tests/drum/ref_import.c", `${L}/src/po32.c`, `${L}/src/po32_patch_import.c`], { stdio: "inherit" });
const r = rng(11), recs = [], names = [], texts1 = [];
const all = sample(dirs.flatMap((d) => walk(d)), want * 2, 5);
for (const f of all) {
  if (recs.length >= want) break;
  const bytes = read(f);
  let pr; try { pr = parseMicrotonic(bytes, f); } catch (_) { continue; }
  const texts = drumTexts(new TextDecoder().decode(bytes));
  if (texts.length !== pr.drums.length) continue;
  const i = Math.floor(r() * texts.length);
  recs.push(texts[i] + "#U " + Array.from(pr.drums[i].p).join(" ") + "\n\f\n"); texts1.push(texts[i]);
  names.push(f.split("/").slice(-2).join("/") + " #" + (i + 1));
}
const out = execFileSync(bin, { input: recs.join(""), maxBuffer: 1 << 26 }).toString().trim().split("\n");
const rows = [];
out.forEach((l, i) => { const v = l.split(" ").map(Number); if (v.length === 4 && v.every((x) => x === x)) rows.push({ name: names[i], dev: v[0], lev: v[1], tail: v[2] }); });
const pct = (a, p) => { const s = a.slice().sort((x, y) => x - y); return s[Math.min(s.length - 1, Math.floor(p * s.length))]; };
const dev = rows.map((x) => x.dev), lev = rows.map((x) => Math.abs(x.lev));
console.log(`test_mtaccuracy: ${rows.length} drums from ${dirs.length} folders (${recs.length - rows.length} silent or refused)`);
console.log(`  spectral deviation  median ${pct(dev, 0.5).toFixed(2)} dB  p90 ${pct(dev, 0.9).toFixed(2)}  max ${Math.max(...dev).toFixed(2)}`);
console.log(`  level deviation     median ${pct(lev, 0.5).toFixed(2)} dB  p90 ${pct(lev, 0.9).toFixed(2)}  max ${Math.max(...lev).toFixed(2)}`);
const bad = rows.filter((x) => x.dev > 2.5 || Math.abs(x.lev) > 2 || x.tail > -60);
console.log(`  outliers (spectral > 2.5 dB, level > 2 dB or a tail above -60 dB): ${bad.length}`);
for (const x of bad.sort((a, b) => b.dev - a.dev).slice(0, 6)) console.log(`    ${x.name}  ${x.dev.toFixed(2)} dB  level ${x.lev.toFixed(2)}  tail ${x.tail}`);
// the outliers again with libpo's own 21 values instead of ours: if they are just as far off, it is the engine's known extremes (20 kHz oscillators, +40 dB EQ, Q 200 at 20 Hz), not the import
{
  const idx = out.map((l, i) => i).filter((i) => rows.find((x) => x.name === names[i]) && bad.includes(rows.find((x) => x.name === names[i])));
  const theirs = libpoParse(idx.map((i) => texts1[i]), root + "build/host/ref_import");
  const rec2 = idx.map((i, k) => (theirs[k] ? texts1[i] + "#U " + Array.from(theirs[k]).join(" ") + "\n\f\n" : ""));
  const out2 = execFileSync(bin, { input: rec2.join(""), maxBuffer: 1 << 26 }).toString().trim().split("\n");
  let alike = 0;
  idx.forEach((i, k) => { const a = rows.find((x) => x.name === names[i]).dev, b = Number((out2[k] || "").split(" ")[0]); if (b === b && (b > 2.5 || Math.abs(a - b) < 1)) alike++; });
  console.log(`  of those, with libpo's own values through our voice: ${alike} of ${idx.length} just as far off (the engine's own extremes, not the import)`);
}
if (rows.length < want * 0.9 || pct(dev, 0.5) > 1.0 || pct(dev, 0.9) > 2.5 || pct(lev, 0.9) > 0.5 || bad.length > rows.length * 0.12) { console.log("FAIL: worse than the engine's own accuracy figures"); process.exit(1); }
console.log("ok");
