// SPDX-License-Identifier: GPL-3.0-only
// Node check of the Projects page's protocol (site/js/fm1proj.js) against a simulated FM-1 that answers like firmware/src/labio.c.
//   node web/test_projects.mjs
import { readFileSync } from "node:fs";
import { Device, looksRight, toBackup, fromBackup } from "../site/js/fm1proj.js";
import { pack7, unpack7 } from "../site/js/fm1ota.js";
import { decodeProject, encodeProject } from "../site/js/drumproj.js";
import { readItem, defaultPlan, applyItem, exportPresets, exportKit, exportPatterns } from "../site/js/imports.js";

let failed = 0;
const ok = (c, w) => { console.log(`${w.padEnd(66)} ${c ? "ok" : "FAIL"}`); if (!c) failed++; };
const sample = (n) => new Uint8Array(readFileSync(new URL("../site/projects/samples/" + n, import.meta.url)));

class FakeLink {                                   // the device side of labio.c: 12 slots, chunk 200 reads, in-order writes
  constructor() { this.slots = Array(12).fill(null); this.q = []; this.px = null; this.open = 0; this.maxIn = 0; }
  drain() { this.q = []; }
  send(frame) {
    const u = unpack7(frame.slice(1, -1));
    const cmd = u[2], n = u[3] | (u[4] << 8), b = u.slice(6, 6 + n);
    const sum = u.slice(6, 6 + n).reduce((a, v) => a + v, 0) & 255;
    if (((~sum) & 255) !== u[6 + n]) return true;               // a bad checksum is ignored
    this.maxIn = Math.max(this.maxIn, n);
    let r;
    if (cmd === 0x50) {
      r = [1, 12, this.open, 0];
      for (const s of this.slots) r.push(s ? 1 : 0, s ? s.length & 255 : 0, s ? s.length >> 8 : 0);
    } else if (cmd === 0x51) {
      const slot = b[0], off = b[1] | (b[2] << 8);
      const s = this.slots[slot];
      if (!s) r = [7, slot, 0, 0, 0, 0];
      else { const k = Math.min(200, s.length - off); r = [0, slot, s.length & 255, s.length >> 8, off & 255, off >> 8, ...s.slice(off, off + k)]; }
    } else if (cmd === 0x52) {
      const slot = b[0], total = b[1] | (b[2] << 8), off = b[3] | (b[4] << 8), data = b.slice(5);
      let st = 0;
      if (off === 0) this.px = { slot, total, got: [] };
      if (!this.px || this.px.got.length !== off) st = 5;
      else {
        this.px.got.push(...data);
        if (this.px.got.length === total) {
          if (total < 4549 || this.px.got[0] < 1 || this.px.got[0] > 3) st = 2;
          else { this.slots[slot] = Uint8Array.from(this.px.got); this.open = slot; st = 1; }
        }
      }
      r = [st, slot, (this.px ? this.px.got.length : 0) & 255, (this.px ? this.px.got.length : 0) >> 8];
    }
    let s = 0; for (const v of r) s += v;
    const m = [0, 0x59, cmd, r.length & 255, r.length >> 8, 0, ...r, ~s & 255];
    this.q.push([0xF0, ...pack7(m), 0xF7]);
    return true;
  }
  async read() { return this.q.shift() || null; }
}

const link = new FakeLink();
const dev = new Device(link);
let info = await dev.info();
ok(info.slots.length === 12 && info.slots.every((s) => !s.used), "info: 12 slots, all empty");
const a = sample("four-on-the-floor.drumlab"), b = sample("two-step.drumlab");
ok(looksRight(a) && looksRight(b) && !looksRight(new Uint8Array(100)) && !looksRight(Uint8Array.from([9, ...new Array(5000).fill(0)])), "looksRight");
ok(await dev.write(3, a) === 1, "write a sample to slot 4: stored");
ok(link.maxIn <= 260, "write frames stay small (" + link.maxIn + " B)");
await dev.write(0, b);
info = await dev.info();
ok(info.slots[3].used && info.slots[3].len === a.length && info.slots[0].used && !info.slots[1].used, "info after writes");
const back = await dev.read(3);
ok(back.length === a.length && back.every((v, i) => v === a[i]), "read slot 4 back: same bytes");
ok(await dev.read(5) === null, "read an empty slot: null");
const file = toBackup([{ slot: 0, bytes: b }, { slot: 3, bytes: a }]);
const list = fromBackup(file);
ok(list.length === 2 && list[1].slot === 3 && list[1].bytes.every((v, i) => v === a[i]), "backup file: round trip");
let bad = false; try { fromBackup('{"chipbox":1}'); } catch (e) { bad = true; }
ok(bad, "a Chipbox file is refused");
let err = null; try { await dev.write(2, Uint8Array.from([9, 1, 2, 3])); } catch (e) { err = e; }
ok(err && err.code === "write" && err.status === 2, "a bad project: the device says 2, we throw");

// import -> write -> read back, the way the Projects page does it: read a slot, change the project, write a slot, read it back
{
  const P = decodeProject(await dev.read(3));
  const donor = decodeProject(b);                                              // another project: its sounds and patterns, shared as Microtonic files and as Drumlab files
  const presets = exportPresets(donor, "donor", 0);
  const it = readItem(presets[1].name, new TextEncoder().encode(presets[1].data));                // sounds 1-8, patch B
  const lines = applyItem(P, it, { ...defaultPlan(it), half: 1, side: 1, first: 4, fit: false });
  ok(lines.length >= 3 && it.kind === "preset", "import: a Microtonic preset into the project read from slot 4, the page's lines say what changed");
  const bytes = encodeProject(P);
  ok(looksRight(bytes) && bytes.length === 5077, "import: the changed project is a valid project for the Projects page and the FM-1's px_ok");
  await dev.write(7, bytes);
  const again = decodeProject(await dev.read(7));
  ok(encodeProject(again).every((v, i) => v === bytes[i]) && again.patches[8][1].every((v, i) => v === donor.patches[0][1][i]), "import: written to slot 8 and read back, the donor's sound 1 patch B is on sound 9 B");
  ok(again.pats[4].trig.some((t) => t & 15) && again.knob[8][1] === 255, "import: the pattern landed on pattern 5 and sound 9 plays patch B");
  info = await dev.info();
  ok(info.slots[7].used && info.slots[3].used && link.slots[3].every((v, i) => v === a[i]), "import: slot 8 is written, slot 4 was not touched");
  const kitf = exportKit(donor, "donor"), patf = exportPatterns(donor, "donor", "all");
  const Q = decodeProject(await dev.read(0));
  applyItem(Q, readItem(kitf.name, new TextEncoder().encode(kitf.data)), {});
  applyItem(Q, readItem(patf.name, new TextEncoder().encode(patf.data)), { first: 0, extras: true });
  await dev.write(9, encodeProject(Q));
  const R = decodeProject(await dev.read(9));
  ok(R.patches.every((two, s2) => two[0].every((v, i) => v === donor.patches[s2][0][i])) && R.pats.every((q, i) => q.trig.every((v, k) => v === donor.pats[i].trig[k])) && R.bpm === donor.bpm, "share files: a kit and patterns from one project, imported into another, written and read back: the same");
}
if (failed) { console.log(failed + " FAILED"); process.exit(1); }
console.log("projects tests passed");
