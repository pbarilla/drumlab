// SPDX-License-Identifier: GPL-3.0-only
// The Projects page, Import and Export: draws what site/js/imports.js works out. Microtonic presets and drum patches, Drumlab kit and pattern files.
// Nothing here writes to the FM-1 by itself: "Write" is a button, it names the slot, and it asks first when the slot holds a project.
import { readItem, defaultPlan, applyItem, title, exportPresets, exportDrums, exportKit, exportPatterns, zipFiles, usedPatterns } from "./imports.js";
import { decodeProject, encodeProject, cloneProject, NPAT } from "./drumproj.js";
import { fromBackup } from "./fm1proj.js";
import { MAX_BYTES } from "./mtpreset.js";

const LETTERS = "ABCDEFGHIJKL";
const MAX_ITEMS = 64;

export function initImport(ctx) {
  const { $, mk, btn, op, say, save, put, refresh, getDev, samples } = ctx;
  let work = null, baseLabel = "", baseSlot = -1, items = [], changed = false, changeLines = [], fileProjects = [];
  let nextId = 1;

  const base = $("base");
  const num = (n, from = 1) => Array.from({ length: n }, (_, i) => i + from);
  const select = (opts, value, onchange) => {                                  // opts: [[value, text]]
    const s = mk("select");
    for (const [v, t] of opts) s.append(new Option(t, String(v)));
    s.value = String(value);
    s.addEventListener("change", () => onchange(s.value));
    return s;
  };
  const check = (text, value, onchange) => {
    const l = mk("label", "chk"), c = mk("input");
    c.type = "checkbox"; c.checked = !!value;
    c.addEventListener("change", () => onchange(c.checked));
    l.append(c, " " + text);
    return l;
  };
  const field = (text, ...els) => { const l = mk("span", "field"); l.append(mk("span", "lab", text), ...els); return l; };

  // ---------------------------------------------------------------- the project to work on
  function rebuildBase() {
    const keep = base.value;
    base.textContent = "";
    const { dev, info } = getDev();
    if (dev && info) {
      const g = mk("optgroup"); g.label = "On the FM-1";
      info.slots.forEach((s, i) => { if (s.used) g.append(new Option(`Project ${i + 1}${i === info.open ? " (open)" : ""}`, "slot:" + i)); });
      if (g.children.length) base.append(g);
    }
    const g2 = mk("optgroup"); g2.label = "Samples";
    samples().forEach((s, i) => g2.append(new Option(s.name, "sample:" + i)));
    if (g2.children.length) base.append(g2);
    if (fileProjects.length) {
      const g3 = mk("optgroup"); g3.label = "Your file";
      fileProjects.forEach((p, i) => g3.append(new Option(p.label, "file:" + i)));
      base.append(g3);
    }
    base.append(new Option("A file…", "pick"));
    if ([...base.options].some((o) => o.value === keep)) base.value = keep; else base.selectedIndex = -1;
    if (!work && base.options.length > 1) {                                      // nothing chosen yet: the open project, or the first sample
      const { info: inf } = getDev();
      const want = inf && inf.slots[inf.open] && inf.slots[inf.open].used ? "slot:" + inf.open : "sample:0";
      if ([...base.options].some((o) => o.value === want)) { base.value = want; loadBase(); }
    }
  }
  async function loadBase() {
    const v = base.value;
    if (v === "pick") { base.selectedIndex = -1; $("basepick").value = ""; $("basepick").click(); return; }
    const [kind, n] = v.split(":");
    const ran = await op(async () => {
      say("");
      let bytes, label, slot = -1;
      const { dev } = getDev();
      if (kind === "slot") { bytes = await dev.read(Number(n), ctx.prog); ctx.prog(null); label = `Project ${Number(n) + 1}`; slot = Number(n); }
      else if (kind === "sample") { const s = samples()[Number(n)]; bytes = new Uint8Array(await (await fetch(s.file)).arrayBuffer()); label = s.name; }
      else { const p = fileProjects[Number(n)]; bytes = p.bytes; label = p.label; }
      if (!bytes) throw Object.assign(new Error("That project is empty."), { code: "badfile" });
      work = decodeProject(bytes); baseLabel = label; baseSlot = slot; changed = false; changeLines = [];
      render();
    });
    if (!ran) setTimeout(loadBase, 150);                                          // the FM-1 was busy with something else: once it's free
  }
  base.addEventListener("change", loadBase);
  $("basepick").addEventListener("change", async (ev) => {
    const f = ev.target.files[0];
    if (!f) return;
    try {
      if (f.size > 1 << 20) throw new Error("big");
      const u8 = new Uint8Array(await f.arrayBuffer());
      let list;
      if (u8[0] === 0x7b) list = fromBackup(new TextDecoder().decode(u8)).map((p) => ({ label: `${f.name}, project ${p.slot + 1}`, bytes: p.bytes }));
      else list = [{ label: f.name, bytes: u8 }];
      list.forEach((p) => decodeProject(p.bytes));                               // refuses what isn't a project
      fileProjects = list;
      rebuildBase();
      base.value = "file:0";
      loadBase();
    } catch (e) { say("That isn't a Drumlab project or backup."); }
  });

  // ---------------------------------------------------------------- the files to import
  async function addFiles(files) {
    const msgs = [];
    for (const f of Array.from(files).slice(0, MAX_ITEMS)) {
      if (items.length >= MAX_ITEMS) { msgs.push("Too many files: the first " + MAX_ITEMS + " are in."); break; }
      try {
        if (f.size > MAX_BYTES) throw Object.assign(new Error("That file is too big. Presets are a few KB."), { code: "badfile" });
        const u8 = new Uint8Array(await f.arrayBuffer());
        if (u8[0] === 0x50 && u8[1] === 0x4b) throw Object.assign(new Error("That's a zip. Unzip it, then pick the files."), { code: "badfile" });
        const it = readItem(f.name, u8);
        it.id = nextId++;
        it.plan = defaultPlan(it);
        if (it.kind === "drum") it.plan.sound = items.filter((x) => x.kind === "drum").length % 16;
        items.push(it);
      } catch (e) { msgs.push(`${f.name}: ${e && e.code === "badfile" ? e.message : "couldn't read it."}`); }
    }
    say(msgs.join(" "));
    render();
  }
  $("importpick").addEventListener("change", (ev) => { addFiles(ev.target.files); ev.target.value = ""; });
  $("pickfiles").addEventListener("click", () => { $("importpick").click(); });
  const drop = $("drop");
  for (const t of ["dragenter", "dragover"]) drop.addEventListener(t, (e) => { e.preventDefault(); drop.classList.add("over"); });
  for (const t of ["dragleave", "drop"]) drop.addEventListener(t, (e) => { e.preventDefault(); drop.classList.remove("over"); });
  drop.addEventListener("drop", (e) => { if (e.dataTransfer && e.dataTransfer.files.length) addFiles(e.dataTransfer.files); });

  // ---------------------------------------------------------------- the list, with what each file does
  function controls(it) {
    const p = it.plan, d = it.data, c = mk("div", "fields");
    const upd = (k) => (v) => { p[k] = typeof v === "boolean" ? v : isNaN(Number(v)) ? v : Number(v); render(); };
    const pat = (k, n) => select(num(NPAT).map((x) => [x - 1, String(x)]).slice(0, n), p[k], upd(k));
    if (it.kind === "preset") {
      const lastFirst = Math.max(0, NPAT - Math.max(1, d.patterns.length));
      c.append(
        field("Sounds", select([[0, "1-8"], [1, "9-16"]], p.half, upd("half"))),
        field("Patch", select([[0, "A"], [1, "B"]], p.side, upd("side"))),
        check("Drums", p.drums, upd("drums")));
      if (d.patterns.length && p.drums) c.append(check("Fit lanes", p.fit, upd("fit")));
      if (d.patterns.length) c.append(
        field("Patterns", select([["none", "None"], ["all", `All (${d.patterns.length})`], ...d.patterns.map((_, i) => [i, "Only " + LETTERS[i]])], p.patterns, upd("patterns"))),
        field("From pattern", select(num(lastFirst + 1).map((x) => [x - 1, String(x)]), p.first, upd("first"))),
        check("Tempo and swing", p.extras, upd("extras")));
    } else if (it.kind === "drum") {
      c.append(field("Sound", select(num(16).map((x) => [x - 1, String(x)]), p.sound, upd("sound"))), field("Patch", select([[0, "A"], [1, "B"]], p.side, upd("side"))));
    } else if (it.kind === "midi") {
      c.append(field("Pattern", pat("pattern", 16)), field("Sounds", select([[0, "1-8"], [1, "9-16"]], p.half, upd("half"))), check("Tempo", p.extras, upd("extras")));
    } else if (it.kind === "patterns") {
      const room = NPAT - (d.patterns[d.patterns.length - 1].n - d.patterns[0].n);
      c.append(field("From pattern", select(num(Math.max(1, room)).map((x) => [x - 1, String(x)]), p.first, upd("first"))), check("Tempo, swing, chain", p.extras, upd("extras")));
    }
    return c;
  }

  function render() {
    const box = $("items");
    box.textContent = "";
    const P = work ? cloneProject(work) : null;
    items.forEach((it) => {
      const row = mk("div", "item");
      const head = mk("div", "ihead");
      head.append(mk("div", "what", title(it)), btn("Remove", "alt", () => { items = items.filter((x) => x !== it); render(); }));
      row.append(head, mk("small", "dim", it.name), controls(it));
      const ul = mk("ul", "notes");
      if (P) { try { for (const l of applyItem(P, it, it.plan)) ul.append(mk("li", "", l)); } catch (e) { ul.append(mk("li", "", "Can't use that file here.")); } }
      else ul.append(mk("li", "", "Pick a project to add it to."));
      row.append(ul);
      box.append(row);
    });
    $("add").hidden = !items.length;
    $("add").disabled = !work;
    $("baseinfo").textContent = work ? `${work.bpm} bpm, ${usedPatterns(work).length} of 16 patterns with hits${changed ? ", with your changes" : ""}` : "";
    $("result").hidden = !changed;
    const ul = $("changes");
    ul.textContent = "";
    for (const l of changeLines) ul.append(mk("li", "", l));
    renderWrite();
    $("exports").classList.toggle("off", !work);
  }

  $("add").addEventListener("click", () => {
    if (!work || !items.length) return;
    const lines = [];
    for (const it of items) {
      try { applyItem(work, it, it.plan).forEach((l) => lines.push(l)); } catch (e) { lines.push(`${it.name}: couldn't add it.`); }
    }
    changeLines = lines; changed = true; items = [];
    say("");
    render();
  });

  // ---------------------------------------------------------------- the result: a file, or the FM-1
  const fileBase = () => String(baseLabel).toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "") || "drumlab";
  $("dl").addEventListener("click", () => { if (work) save(`${fileBase()}-import.drumlab`, encodeProject(work), "application/octet-stream"); });
  $("startover").addEventListener("click", () => { changed = false; changeLines = []; items = []; if (base.value) loadBase(); else render(); });

  function renderWrite() {
    const box = $("writebox");
    box.textContent = "";
    const { dev, info } = getDev();
    if (!dev || !info || !changed) return;
    const sel = mk("select");
    info.slots.forEach((s, i) => sel.append(new Option(`Project ${i + 1}${s.used ? " (replace)" : " (empty)"}`, String(i))));
    sel.value = String(baseSlot >= 0 ? baseSlot : info.slots.findIndex((s) => !s.used) >= 0 ? info.slots.findIndex((s) => !s.used) : 0);
    const go = btn("Write to project " + (Number(sel.value) + 1), "", () => {
      const slot = Number(sel.value);
      const doIt = () => op(async () => { await put(slot, encodeProject(work)); await refresh(); say(`Written to project ${slot + 1}. It's open on the FM-1 now.`); });
      if (!info.slots[slot].used) { doIt(); return; }
      const was = [...box.childNodes];
      box.textContent = "";
      box.append(mk("span", "small", `Replaces project ${slot + 1} on the FM-1 and opens it.`), " ", btn("Replace", "warn", doIt), " ", btn("Cancel", "alt", () => renderWrite()));
      void was;
    });
    sel.addEventListener("change", () => { go.textContent = "Write to project " + (Number(sel.value) + 1); });
    box.append(sel, go);
  }

  // ---------------------------------------------------------------- export
  const first = $("expfirst");
  for (let i = 0; i + 12 <= NPAT; i++) first.append(new Option(`${i + 1}-${i + 12}`, String(i)));
  const pick = $("exppat");
  pick.append(new Option("All 16", "all"));
  for (let i = 0; i < NPAT; i++) pick.append(new Option(`Pattern ${i + 1}`, String(i)));
  const out = (fn) => () => { if (!work) return; try { fn(); say(""); } catch (e) { say("Couldn't make that file."); } };
  $("exppresets").addEventListener("click", out(() => save(`${fileBase()}-microtonic-presets.zip`, zipFiles(exportPresets(work, fileBase(), Number(first.value))), "application/zip")));
  $("expdrums").addEventListener("click", out(() => save(`${fileBase()}-microtonic-drums.zip`, zipFiles(exportDrums(work, fileBase())), "application/zip")));
  $("expkit").addEventListener("click", out(() => { const f = exportKit(work, baseLabel); save(f.name, f.data, "application/json"); }));
  $("exppattern").addEventListener("click", out(() => { const f = exportPatterns(work, baseLabel, pick.value === "all" ? "all" : Number(pick.value)); save(f.name, f.data, "application/json"); }));

  rebuildBase();
  render();
  return { rebuildBase, render, current: () => work };
}
