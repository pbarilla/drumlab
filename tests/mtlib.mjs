// SPDX-License-Identifier: GPL-3.0-only
// Shared by the Microtonic import tests: finds the local sample folders (never copied into the repo), samples them with a fixed seed, and runs libpo's importer.
import { readFileSync, readdirSync, statSync, existsSync } from "node:fs";
import { execFileSync } from "node:child_process";
import { homedir } from "node:os";
import { join } from "node:path";

export const HOME = homedir();
export const SAMPLE_DIRS = (process.env.MT_SAMPLES ? process.env.MT_SAMPLES.split(":") : [join(HOME, "Downloads", "Patternarium G01-G25"), join(HOME, "Downloads", "SonicCharge.1550.presets.for.MicroTonic")]).filter((d) => existsSync(d));
export const EXTRA_DIRS = (process.env.MT_WEB ? process.env.MT_WEB.split(":") : []).filter((d) => existsSync(d));
const EXT = /\.(mtpreset|mtdrum|mtpg|mtdp)$/i;

export function walk(dir, out = []) {
  let list; try { list = readdirSync(dir); } catch (_) { return out; }
  for (const n of list) {
    if (n.startsWith(".")) continue;
    const p = join(dir, n);
    let st; try { st = statSync(p); } catch (_) { continue; }
    if (st.isDirectory()) walk(p, out); else if (EXT.test(n)) out.push(p);
  }
  return out;
}
export function rng(seed) { let s = seed >>> 0; return () => { s = (Math.imul(s, 1664525) + 1013904223) >>> 0; return s / 4294967296; }; }
export function sample(list, n, seed = 1) {
  const r = rng(seed), a = list.slice();
  for (let i = a.length - 1; i > 0; i--) { const j = Math.floor(r() * (i + 1)); [a[i], a[j]] = [a[j], a[i]]; }
  return a.slice(0, n);
}

// the drum blocks of a file as libpo wants them: "Key: value" lines (found without the importer under test)
export function drumTexts(text) {
  const lines = text.replace(/\r\n?/g, "\n").split("\n");
  const out = [];
  let inPat = false;
  for (let i = 0; i < lines.length; i++) {
    const l = lines[i];
    if (/^\s*(Patterns\s*[:=]\s*\{|Pattern\[\d+\]\s*=\s*\{)/.test(l)) inPat = true;
    const m = !inPat && /^(\s*)("?\d"?\s*[:=]|Channel\[\d+\]\s*=)\s*\{\s*$/.exec(l);
    if (m) {
      const block = [];
      for (i++; i < lines.length && !(lines[i].startsWith(m[1] + "}") && lines[i].trim() === "}"); i++) block.push(lines[i].replace(/^(\s*"?\w+"?\s*)=/, "$1:"));
      out.push(block.join("\n") + "\n");
    }
  }
  if (!out.length && /^\s*(MicrotonicDrumPatch|OscWave|Osc)/m.test(text)) out.push(lines.join("\n").replace(/^(\s*"?\w+"?\s*)=/gm, "$1:") + "\n");
  return out;
}

// libpo through tests/drum/ref_import.c: array of Uint16Array or null (its parser refused)
export function libpoParse(texts, refBin) {
  const input = texts.map((t) => t + "\f\n").join("");
  const res = execFileSync(refBin, { input, maxBuffer: 1 << 28 }).toString().trim().split("\n");
  return res.map((l) => (l === "ERR" ? null : Uint16Array.from(l.split(" ").map(Number))));
}
export const read = (p) => new Uint8Array(readFileSync(p));
