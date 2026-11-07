// SPDX-License-Identifier: GPL-3.0-only
// The Projects page: download and upload a slot, back up and restore all of them, send a sample.
import { Device, looksRight, toBackup, fromBackup } from "./fm1proj.js";

const $ = (id) => document.getElementById(id);
const TEXT = {
  nomidi: "This browser can't talk to the FM-1.",
  denied: "MIDI was blocked. Allow it for this site and try again.",
  notfound: "No Drumlab found. Check the USB cable and close other MIDI apps.",
  lost: "Lost the FM-1. Replug it and connect again.",
  read: "Couldn't read it.",
  w2: "That isn't a project Drumlab can read.",
  w4: "Couldn't write it. Try again.",
  w5: "Something went out of order. Try again.",
  w6: "No such slot.",
  badfile: "That file isn't a Drumlab project.",
};
const say = (t) => { $("status").textContent = t; };
const prog = (f) => { const b = $("bar"); b.style.display = f == null ? "none" : "block"; if (f != null) b.value = Math.round(f * 100); };
const fail = (e) => {
  prog(null);
  if (e && e.code === "write") say(TEXT["w" + e.status] || TEXT.w4);
  else if (e && TEXT[e.code]) say(TEXT[e.code]);
  else if (e && e.name === "SecurityError") say(TEXT.denied);
  else say(e && e.message ? e.message : String(e));
};

let busy = false;
const op = async (fn) => {                      // one thing at a time: the FM-1 takes one transfer at once
  if (busy) return;
  busy = true; document.body.classList.add("busy");
  try { await fn(); } catch (e) { fail(e); } finally { busy = false; document.body.classList.remove("busy"); }
};
let dev = null, info = null, samples = [], pending = null;     // pending: { slot } or { restore: true } for the file chooser

const save = (name, data, type) => {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([data], { type }));
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 2000);
};
const size = (n) => (n < 1024 ? n + " B" : (n / 1024).toFixed(1) + " KB");
const firstFree = () => { for (let s = 0; s < info.slots.length; s++) if (!info.slots[s].used) return s; return 0; };
const mk = (tag, cls, text) => { const e = document.createElement(tag); if (cls) e.className = cls; if (text != null) e.textContent = text; return e; };
const btn = (text, cls, fn) => { const b = mk("button", cls, text); b.addEventListener("click", fn); return b; };

async function refresh() {
  info = await dev.info();
  $("mine").hidden = false;
  const box = $("slots");
  box.textContent = "";
  info.slots.forEach((s, i) => {
    const row = mk("div", "row" + (i === info.open ? " open" : ""));
    const what = mk("div", "what", "Project " + (i + 1));
    what.append(mk("small", "", (s.used ? size(s.len) : "empty") + (i === info.open ? (info.dirty ? ", open, unsaved" : ", open") : "")));
    row.append(mk("span", "n", String(i + 1)), what);
    const b = mk("div", "btns");
    if (s.used) b.append(btn("Download", "alt", () => download(i)));
    b.append(btn(s.used ? "Replace…" : "Upload…", "alt", () => chooseFor(i, b)));
    row.append(b);
    box.append(row);
  });
  renderSamples();
}

const download = (i) => op(async () => {
  say("");
  const bytes = await dev.read(i, prog);
  prog(null);
  save("project-" + (i + 1) + ".drumlab", bytes, "application/octet-stream");
});

function chooseFor(slot, b) {
  const open = () => { pending = { slot }; $("pick").value = ""; $("pick").accept = ".drumlab"; $("pick").click(); };
  if (!info.slots[slot].used) { open(); return; }
  const was = [...b.childNodes];
  b.textContent = "";
  b.append(mk("span", "small", `Replaces project ${slot + 1}.`), btn("Pick a file", "warn", open), btn("Cancel", "alt", () => { b.textContent = ""; b.append(...was); }));
}

$("pick").addEventListener("change", async (ev) => {
  const f = ev.target.files[0];
  if (!f || !pending) return;
  const p = pending;
  pending = null;
  try {
    if (p.restore) { askRestore(fromBackup(await f.text())); return; }
    const bytes = new Uint8Array(await f.arrayBuffer());
    if (!looksRight(bytes)) { fail({ code: "badfile" }); return; }
    op(async () => { await put(p.slot, bytes); await refresh(); });
  } catch (_) { fail({ code: "badfile" }); }
});

async function put(slot, bytes) {
  await dev.write(slot, bytes, prog);
  prog(null);
}

function askRestore(list) {                     // the warning only when something would really be replaced
  list = list.filter((p) => p.slot >= 0 && p.slot < info.slots.length && looksRight(p.bytes));
  if (!list.length) { fail({ code: "badfile" }); return; }
  const used = list.filter((p) => info.slots[p.slot].used).length;
  const go = () => { $("confirm").hidden = true; op(async () => { for (const p of list) await put(p.slot, p.bytes); await refresh(); }); };
  if (!used) { go(); return; }
  const c = $("confirm");
  c.textContent = `This replaces ${used === 1 ? "1 project" : used + " projects"}. `;
  c.append(btn("Replace", "warn", go), " ", btn("Cancel", "alt", () => { c.hidden = true; }));
  c.hidden = false;
}

$("backup").addEventListener("click", () => op(async () => {
  const out = [];
  for (let i = 0; i < info.slots.length; i++) {
    if (!info.slots[i].used) continue;
    out.push({ slot: i, bytes: await dev.read(i, prog) });
  }
  prog(null);
  if (!out.length) { say("Nothing to back up."); return; }
  save(`drumlab-backup-${new Date().toISOString().slice(0, 10)}.json`, toBackup(out), "application/json");
}));

$("restore").addEventListener("click", () => {
  $("confirm").hidden = true;
  pending = { restore: true };
  $("pick").value = "";
  $("pick").accept = ".json,application/json";
  $("pick").click();
});

function renderSamples() {
  const box = $("samples");
  box.textContent = "";
  for (const s of samples) {
    const row = mk("div", "row");
    const what = mk("div", "what", s.name);
    if (s.note) what.append(mk("small", "", s.note));
    const b = mk("div", "btns");
    const a = mk("a", "btn alt", "Download");
    a.href = s.file;
    a.download = s.file.split("/").pop();
    b.append(a);
    if (dev && info) {
      const sel = mk("select");
      for (let i = 0; i < info.slots.length; i++) sel.append(new Option(`Project ${i + 1}${info.slots[i].used ? " (replace)" : ""}`, String(i)));
      sel.value = String(firstFree());
      const send = btn("Send", "", () => sendSample(s, sel, send));
      sel.addEventListener("change", () => { send.textContent = "Send"; send.className = ""; send.armed = false; });
      b.append(sel, send);
    }
    row.append(what, b);
    box.append(row);
  }
}

function sendSample(s, sel, send) {
  const slot = Number(sel.value);
  if (info.slots[slot].used && !send.armed) { send.armed = true; send.textContent = `Replace project ${slot + 1}?`; send.className = "warn"; return; }
  op(async () => {
    const r = await fetch(s.file);
    await put(slot, new Uint8Array(await r.arrayBuffer()));
    await refresh();
  });
}

$("connect").addEventListener("click", () => {
  if (!navigator.requestMIDIAccess) { say(TEXT.nomidi); return; }
  op(async () => {
    say("");
    try {
      dev = await Device.connect();
      await refresh();
      $("connect").textContent = "Refresh";
    } catch (e) { dev = null; throw e; }
  });
});

(async () => {
  try { samples = await (await fetch("samples.json")).json(); } catch (_) { samples = []; }
  renderSamples();
})();
