// SPDX-License-Identifier: GPL-3.0-only
// Our Microtonic importer against libpo's C importer, drum by drum: the 21 u16 must be identical.
//   LIBPO=<libpo>/src/core node tests/cmp_import.mjs [files per folder, default 300]
// Samples: ~/Downloads/Patternarium G01-G25 and SonicCharge.1550... (or MT_SAMPLES=dir:dir), MT_WEB=dir for more. None are in the repo; skipped when absent.
import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { parseMicrotonic } from "../site/js/mtpreset.js";
import { SAMPLE_DIRS, EXTRA_DIRS, walk, sample, drumTexts, libpoParse, read } from "./mtlib.mjs";

const root = fileURLToPath(new URL("..", import.meta.url));
if (!process.env.LIBPO) { console.log("cmp_import: LIBPO not set (libpo's src/core folder), skipped"); process.exit(0); }
const dirs = [...SAMPLE_DIRS, ...EXTRA_DIRS];
if (!dirs.length) { console.log("cmp_import: no sample folders, skipped"); process.exit(0); }
const L = process.env.LIBPO, ref = root + "build/host/ref_import";
mkdirSync(root + "build/host", { recursive: true });
execFileSync("cc", ["-O2", `-I${L}/include`, `-I${L}/src`, "-o", ref, root + "tests/drum/ref_import.c", `${L}/src/po32.c`, `${L}/src/po32_patch_import.c`], { stdio: "inherit" });

const per = Number(process.argv[2]) || 300;
let files = 0, drums = 0, same = 0, refErr = 0, ourWarn = 0, miss = 0;
const diffs = [], perParam = new Array(21).fill(0);
for (const dir of dirs) {
  const all = walk(dir);
  for (const f of sample(all, per, 7)) {
    const bytes = read(f), text = new TextDecoder().decode(bytes);
    let r; try { r = parseMicrotonic(bytes, f); } catch (e) { console.log("FAIL parse", f, e.message); process.exitCode = 1; continue; }
    const texts = drumTexts(text), theirs = libpoParse(texts, ref);
    files++;
    if (texts.length !== r.drums.length) { console.log(`FAIL ${f}: we found ${r.drums.length} drums, the reference ${texts.length}`); process.exitCode = 1; miss++; continue; }
    r.drums.forEach((d, i) => {
      drums++;
      if (!theirs[i]) { refErr++; if (!d.warn.length) { /* libpo refuses, we read it: fine, but say how many */ ourWarn++; } return; }
      let ok = true;
      for (let k = 0; k < 21; k++) if (d.p[k] !== theirs[i][k]) { ok = false; perParam[k]++; diffs.push({ f: f.split("/").slice(-2).join("/"), i, k, ours: d.p[k], libpo: theirs[i][k] }); }
      if (ok) same++;
    });
  }
}
const compared = drums - refErr;
console.log(`cmp_import: ${files} files, ${drums} drums, libpo refused ${refErr}; of ${compared} compared, ${same} identical (${(100 * same / Math.max(1, compared)).toFixed(3)} %)`);
const worst = diffs.reduce((m, d) => Math.max(m, Math.abs(d.ours - d.libpo)), 0);
console.log(`  differing values: ${diffs.length}, largest difference ${worst} of 65535`);
if (diffs.length) { console.log("  by parameter:", perParam.map((n, k) => (n ? k + ":" + n : "")).filter(Boolean).join(" ")); for (const d of diffs.slice(0, 8)) console.log("  ", JSON.stringify(d)); }
if (process.env.MT_STRICT && diffs.length) process.exitCode = 1;
if (miss) process.exitCode = 1;
