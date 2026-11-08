// SPDX-License-Identifier: GPL-3.0-only
// The Projects page's import and export, without the page: files in, a plan per file, a project changed, the plain-words lines that say what changed
// and what was lost, and the export files. projects.js only draws it. Tested in tests/test_mtimport.mjs and web/test_projects.mjs.
import { parseMicrotonic, putDrums, mapPatterns, fitLanes, isIdentity, presetFromProject, writeDrum, MAX_BYTES } from "./mtpreset.js";
import { readKitFile, readPatternFile, putKit, putPatterns, kitFile, patternFile, cloneProject, hitCount, ranges, NS, NPAT } from "./drumproj.js";
import { zip } from "./zip.js";

const bad = (m) => { const e = new Error(m); e.code = "badfile"; return e; };
const LETTERS = "ABCDEFGHIJKL";
export const SIDE = ["A", "B"];

// a picked file -> an item: { name, kind: "preset" | "drum" | "midi" | "kit" | "patterns", data }
export function readItem(name, bytes) {
  if (!(bytes instanceof Uint8Array)) throw bad("That isn't a file Drumlab can read.");
  if (bytes.length > MAX_BYTES) throw bad("That file is too big. Presets are a few KB.");
  if (!bytes.length) throw bad("That file is empty.");
  let i = 0;
  while (i < bytes.length && (bytes[i] === 32 || bytes[i] === 10 || bytes[i] === 13 || bytes[i] === 9)) i++;
  if (bytes[i] === 0x7b) {                                                       // "{": one of ours (or a backup)
    let o = null;
    try { o = JSON.parse(new TextDecoder().decode(bytes)); } catch (_) { /* not JSON: a Microtonic file also starts with a brace line, below */ }
    if (o && typeof o === "object") {
      if (o["drumlab-kit"]) return { name, kind: "kit", data: readKitFile(JSON.stringify(o)) };
      if (o["drumlab-pattern"]) return { name, kind: "patterns", data: readPatternFile(JSON.stringify(o)) };
      if (o.drumlab) throw bad("That's a backup. Use Restore backup, or pick a project as the starting point.");
      throw bad("That isn't a Microtonic or Drumlab file.");
    }
  }
  const m = parseMicrotonic(bytes, name);
  return { name, kind: m.kind, data: m };
}

export function defaultPlan(item) {
  const d = item.data;
  if (item.kind === "preset") return { drums: true, half: 0, side: 0, patterns: d.patterns.some((p) => p.ch.some((c) => c.some((v) => v & 1))) ? "all" : "none", first: 0, extras: true, fit: true };
  if (item.kind === "drum") return { sound: 0, side: 0 };
  if (item.kind === "midi") return { pattern: 0, half: 0, extras: false };
  if (item.kind === "patterns") return { first: d.patterns[0].n, extras: true };
  return {};
}

export const title = (item) => {
  const d = item.data;
  if (item.kind === "preset") return `${d.name}: ${d.drums.length} drums, ${d.patterns.filter((p) => p.ch.some((c) => c.some((v) => v & 1))).length} patterns with hits`;
  if (item.kind === "drum") return `${d.name}: 1 drum patch`;
  if (item.kind === "midi") return `${d.name}: a pattern, ${d.patterns[0].ch.reduce((n, c) => n + c.filter((v) => v & 1).length, 0)} hits`;
  if (item.kind === "kit") return `${d.name}: a kit, 16 sounds with A and B`;
  return `${d.name}: ${d.patterns.length} pattern${d.patterns.length === 1 ? "" : "s"}`;
};
const soundRange = (half) => (half ? "9-16" : "1-8");
const warnLine = (drums) => {
  const w = [];
  drums.forEach((d, i) => { if (d.warn.length) w.push(`drum ${i + 1} ${d.warn.slice(0, 2).join(", ")}`); });
  return w.length ? `Couldn't read ${w.slice(0, 3).join("; ")}${w.length > 3 ? "; more" : ""}. Left at 0.` : null;
};

// Does it. Changes P, returns the lines (plain words) for the page: what changed, then what is lost.
export function applyItem(P, item, plan) {
  const d = item.data, L = [], lost = [];
  if (item.kind === "preset") {
    const first = 8 * plan.half, withPat = plan.patterns !== "none" && d.patterns.length;
    const which = plan.patterns === "all" ? "all" : plan.patterns;
    const order = plan.drums && withPat && plan.fit ? fitLanes(d, which) : undefined;
    if (plan.drums) {
      putDrums(P, d.drums, d.drums.map((_, i) => first + (order ? order[i] : i)), plan.side);
      L.push(`Drums 1-${d.drums.length} go to sounds ${soundRange(plan.half)}, patch ${SIDE[plan.side]}. Those sounds play patch ${SIDE[plan.side]} now.`);
      if (order && !isIdentity(order)) L.push("Lanes fitted, so these drums moved: " + d.drums.map((_, i) => (order[i] !== i ? `${i + 1} to sound ${first + order[i] + 1}` : null)).filter(Boolean).join(", ") + ".");
      lost.push("choke, pan and stereo noise");
      const w = warnLine(d.drums); if (w) L.push(w);
    }
    if (withPat) {
      const r = mapPatterns(P, d, { half: plan.half, which, first: plan.first, extras: plan.extras, order });
      if (r.placed.length) {
        L.push(`Pattern${r.placed.length > 1 ? "s" : ""} ${plan.patterns === "all" ? `A-${LETTERS[d.patterns.length - 1]}` : LETTERS[plan.patterns]} go${r.placed.length > 1 ? "" : "es"} to pattern${r.placed.length > 1 ? "s" : ""} ${ranges(r.placed)}. What was there is replaced.`);
        if (plan.extras) L.push(`Tempo ${P.bpm}, swing ${P.swing} of 12${P.chainN > 1 ? `, chain of ${P.chainN}` : ""}.`);
        L.push("A lane plays one hit a step" + (order && !isIdentity(order) ? "." : ": when drums 1 and 5 (2 and 6, 3 and 7, 4 and 8) both hit, the lower one stays.") + (r.dropped ? ` ${r.dropped} hit${r.dropped === 1 ? "" : "s"} dropped.` : ""));
        if (r.spread) L.push(`Accents go by step, so an accent lifts every hit on it (${r.spread} step${r.spread === 1 ? "" : "s"} lifted hits that had none).`);
        if (r.fills) lost.push(`fills (${r.fills} step${r.fills === 1 ? "" : "s"})`);
        if (r.muted) lost.push(`${r.muted} hit${r.muted === 1 ? "" : "s"} of muted drums`);
        if (d.patterns.some((p) => p.steps < 16)) lost.push("short patterns are repeated to 16 steps");
        lost.push(...r.notes);
      }
    }
  } else if (item.kind === "drum") {
    putDrums(P, d.drums, [plan.sound], plan.side);
    L.push(`Goes to sound ${plan.sound + 1}, patch ${SIDE[plan.side]}. Sound ${plan.sound + 1} plays patch ${SIDE[plan.side]} now.`);
    const w = warnLine(d.drums); if (w) L.push(w);
    lost.push("choke, pan and stereo noise");
  } else if (item.kind === "midi") {
    const r = mapPatterns(P, d, { half: plan.half, which: 0, first: plan.pattern, extras: plan.extras });
    L.push(`Goes to pattern ${plan.pattern + 1}, for sounds ${soundRange(plan.half)}. What was there is replaced.`);
    L.push("A lane plays one hit a step: when drums 1 and 5 (2 and 6, 3 and 7, 4 and 8) both hit, the lower one stays" + (r.dropped ? ` (${r.dropped} hit${r.dropped === 1 ? "" : "s"} dropped).` : "."));
    lost.push("velocity (100 and up is an accent, per step)", ...r.notes, ...d.notes);
  } else if (item.kind === "kit") {
    putKit(P, d);
    L.push("All 16 sounds, patch A and B, knobs and names are replaced.");
  } else {
    const placed = putPatterns(P, d, plan.first, plan.extras);
    L.push(`Pattern${placed.length === 1 ? "" : "s"} ${ranges(placed)} replaced.` + (plan.extras ? ` Tempo ${P.bpm}, swing ${P.swing} of 12${P.chainN > 1 ? `, chain of ${P.chainN}` : ""}.` : ""));
  }
  if (lost.length) L.push("Not kept: " + lost.join("; ") + ".");
  return L;
}

// the lines for a plan, without changing anything
export function describe(P, item, plan) { return applyItem(cloneProject(P), item, plan); }

// ======================================================================= export
const safe = (s) => String(s || "drumlab").replace(/[^A-Za-z0-9._-]+/g, "-").replace(/^-+|-+$/g, "").slice(0, 40) || "drumlab";
// Microtonic presets: 4 files, sounds 1-8 / 9-16 x patch A / B, each with patterns first+1 .. first+12 for its 8 sounds
export function exportPresets(P, base, first = 0, patterns = true) {
  const b = safe(base), out = [];
  for (const half of [0, 1]) for (const side of [0, 1]) out.push({ name: `${b}-sounds-${soundRange(half)}-${SIDE[side]}.mtpreset`, data: presetFromProject(P, { half, side, first, name: b, patterns }) });
  return out;
}
export function exportDrums(P, base) {
  const b = safe(base), out = [];
  for (let s = 0; s < NS; s++) for (let q = 0; q < 2; q++) out.push({ name: `${b}-sound-${String(s + 1).padStart(2, "0")}-${SIDE[q]}.mtdrum`, data: writeDrum(P.patches[s][q], `${b} ${s + 1}${SIDE[q]}`) });
  return out;
}
export const exportKit = (P, base) => ({ name: `${safe(base)}.drumlab-kit.json`, data: kitFile(P, base) });
export function exportPatterns(P, base, which) {       // which: "all" or a pattern number 0..15
  const list = which === "all" ? Array.from({ length: NPAT }, (_, i) => i) : [which];
  return { name: `${safe(base)}.drumlab-${which === "all" ? "patterns" : "pattern-" + (which + 1)}.json`, data: patternFile(P, list, base) };
}
export const zipFiles = (files) => zip(files);
export const usedPatterns = (P) => P.pats.map((q, i) => (hitCount(q) ? i : -1)).filter((i) => i >= 0);
