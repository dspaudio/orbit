// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// installer.js against a fake document, a fake Web MIDI permission and a scripted Updater:
// no browser, no hardware, no timers of its own. The last case drives the real fm1ota.js
// Updater against the simulated FM-1 of test_web.mjs through the page. Run from anywhere:
//   node web/test_installer.mjs

import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { pack7, unpack7, Updater } from "./fm1ota.js";
import { validatePackage } from "./fm1pkg.js";
import { describeDevice, describeError, ERRORS, mount, packageFor, parseManifest, preflight, progressOf } from "./installer.js";

const HERE = dirname(fileURLToPath(import.meta.url));
const failures = [];
const ok = (cond, label) => {
  console.log(`${label.padEnd(72)} ${cond ? "ok" : "FAIL"}`);
  if (!cond) failures.push(label);
};
const eq = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
const hex = (bytes) => createHash("sha256").update(bytes).digest("hex");

/* ------------------------------------------------------------ a package, as fm1pkg_make writes it --- */
const BLOCKS = 20, BLK = 0x30, KEEP = 0x2F;
const LOADER_MARK = new TextEncoder().encode("FELUCCA-LOADER-1");
const crc16 = (data) => {
  let crc = 0;
  for (const b of data) {
    crc ^= b << 8;
    for (let i = 0; i < 8; i++) crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xFFFF : (crc << 1) & 0xFFFF;
  }
  return crc;
};
const enc = (data) => {
  const out = Uint8Array.from(data);
  let key = 0xFFFF;
  for (let i = 0; i < out.length; i++) {
    out[i] ^= key & 0xFF;
    key = ((key << 1) ^ (key & 0x8000 ? 0x1021 : 0)) & 0xFFFF;
  }
  return out;
};
const put16 = (a, o, v) => { a[o] = v & 0xFF; a[o + 1] = (v >>> 8) & 0xFF; };
const put32 = (a, o, v) => { for (let i = 0; i < 4; i++) a[o + i] = Math.floor(v / 2 ** (8 * i)) & 0xFF; };

function makePackage({ product = "FM-1_900", marker = true, size = 0x2000, damage = false } = {}) {
  const img = Uint8Array.from({ length: size }, (_, i) => (i * 7) & 0xFF);
  img.set(marker ? LOADER_MARK : new Uint8Array(16), 0x1800);
  const data = img.subarray(0x400);
  const entry = new Uint8Array(0x50);
  put16(entry, 4, crc16(data));
  put32(entry, 8, 0x400);
  put32(entry, 12, data.length);
  entry.set(new TextEncoder().encode("flash.bin"), 0x40);
  const list = enc(entry);
  const header = new Uint8Array(0x40);
  put32(header, 4, size);
  put16(header, 8, 1);
  put16(header, 2, crc16(list));
  put16(header, 0, crc16(header.subarray(2)));
  img.set(enc(header), 0);
  img.set(list, 0x40);
  if (damage) img[0x1000] ^= 1;
  const raw = new Uint8Array(size + BLOCKS);
  for (let i = 0; i < BLOCKS; i++) {
    raw.set(img.subarray(i * KEEP, (i + 1) * KEEP), i * BLK);
    raw[i * BLK + KEEP] = i < product.length ? (product.charCodeAt(i) + i + 1) & 0xFF : 0x7D;
  }
  raw.set(img.subarray(BLOCKS * KEEP), BLOCKS * BLK);
  return raw;
}

/* ------------------------------------------------------------------------------- fake document --- */
class El {
  constructor(id) {
    this.id = id; this.text = ""; this.history = []; this.disabled = false; this.hidden = false; this.value = "";
    this.files = []; this.attrs = {}; this.listeners = {}; this.dataset = {}; this.children = [];
  }
  get textContent() { return this.text; }
  set textContent(value) { this.text = String(value); this.history.push(this.text); }
  addEventListener(type, fn) { (this.listeners[type] ||= []).push(fn); }
  setAttribute(name, value) { this.attrs[name] = String(value); }
  removeAttribute(name) { delete this.attrs[name]; }
  getAttribute(name) { return name in this.attrs ? this.attrs[name] : null; }
  replaceChildren(...children) { this.children = children; }
  dispatch(type) { return Promise.all((this.listeners[type] || []).map((fn) => fn({ type, target: this }))); }
  click() { return this.disabled ? Promise.resolve() : this.dispatch("click"); }
}

const PAGE = readFileSync(join(HERE, "installer.html"), "utf8");
const PAGE_TAGS = [...PAGE.matchAll(/<(\w+)([^>]*)\sid="([^"]+)"([^>]*)>/g)].map((m) => ({ id: m[3], attrs: `${m[2]} ${m[4]} ` }));
const PAGE_LABELS = Object.fromEntries([...PAGE.matchAll(/<button id="(\w+)"[^>]*>([^<]*)</g)].map((m) => [m[1], m[2]]));

class FakeDocument {
  constructor() {
    this.ids = {};
    for (const { id, attrs } of PAGE_TAGS) {
      const el = new El(id);
      el.ownerDocument = this;
      el.hidden = /\shidden\s/.test(attrs);
      el.disabled = /\sdisabled\s/.test(attrs);
      this.ids[id] = el;
    }
    for (const [id, label] of Object.entries(PAGE_LABELS)) this.ids[id].textContent = label;
    this.body = { dataset: { manifest: "manifest.json" } };
    this.baseURI = "https://orbit.test/site/index.html";
  }
  getElementById(id) { return this.ids[id] || null; }
  createElement(tag) { return new El(tag); }
}

/* --------------------------------------------------------------------- fakes for the page's deps --- */
const parseId = (text) => { const m = /^([^_]+)_(\d+)$/.exec(text); return { text, model: m[1], version: parseInt(m[2], 10) }; };

class FakeUpdater {
  static instances = [];
  constructor(access) { this.access = access; this.script = access.script; this.calls = []; FakeUpdater.instances.push(this); }
  async find(filter) {
    this.calls.push(["find", filter ? "filtered" : "any"]);
    if (!this.script.identity) return null;
    const id = parseId(this.script.identity);
    if (filter && !filter(id)) return null;
    return { id, name: "FM-1", link: { close: () => this.calls.push(["close"]) } };
  }
  run(step) {
    const s = this.script;
    for (const [kind, arg] of s.steps || []) { step(kind, arg); if (s.onStep) s.onStep(kind, arg); }
    if (s.error) throw Object.assign(new Error(s.error.message), { code: s.error.code, detail: s.error.detail || "" });
  }
  async install(image, product, step, anyModel) { this.calls.push(["install", product, image.length]); this.image = image.slice(); this.anyModel = anyModel; this.run(step); return product; }
  async resume(image, step, opts) { this.calls.push(["resume", opts]); this.run(step); return this.script.resumed !== false; }
}

const midiGranted = (script) => {
  const calls = [];
  return { calls, access: { script }, request: (opts) => { calls.push(opts); return Promise.resolve({ script }); } };
};
const midiRejected = (name) => {
  const calls = [];
  return { calls, request: (opts) => { calls.push(opts); return Promise.reject(Object.assign(new Error(`${name}: permission`), { name })); } };
};
const midiMissing = () => {
  const calls = [];
  return { calls, request: () => { calls.push("sync"); throw new Error("navigator.requestMIDIAccess is not available"); } };
};

const body = (bytes) => ({ ok: true, status: 200, arrayBuffer: async () => bytes.slice().buffer });
const json = (obj) => ({ ok: true, status: 200, json: async () => JSON.parse(JSON.stringify(obj)) });
const missing = { ok: false, status: 404, json: async () => { throw new Error("404"); }, arrayBuffer: async () => { throw new Error("404"); } };
const fetchFor = (routes) => {
  const log = [], options = [];
  return { log, options, routes, fetch: async (url, opts) => { log.push(url); options.push(opts); return routes[url] || missing; } };
};

const VALID = makePackage();
const MANIFEST = {
  name: "ORBIT", version: "0.6.0", product: "FM-1_900", package: "firmware/orbit-0.6.0.fwsc",
  size: VALID.length, sha256: hex(VALID), release_url: "https://github.com/dspaudio/orbit/releases/tag/v0.6.0",
};
const SITE = "https://orbit.test/site/";

async function page({ midi = midiGranted({ identity: "FM-1_015" }), routes = {}, Updater = FakeUpdater } = {}) {
  const doc = new FakeDocument();
  const net = fetchFor({ ...routes });
  const ctl = mount(doc, { requestMIDIAccess: midi.request, fetch: net.fetch, Updater });
  await ctl.ready;
  return { doc, ui: doc.ids, ctl, midi, net };
}
const released = { [`${SITE}manifest.json`]: json(MANIFEST), [`${SITE}firmware/orbit-0.6.0.fwsc`]: body(VALID) };


async function download(p, bytes = VALID) {
  Object.assign(p.net.routes, {
    [`${SITE}manifest.json`]: json({ ...MANIFEST, size: bytes.length, sha256: hex(bytes) }),
    [`${SITE}firmware/orbit-0.6.0.fwsc`]: body(bytes),
  });
  await p.ctl.useRelease();
}

/* ---------------------------------------------------------------------------------------- cases --- */
async function pureFunctions() {
  ok(Object.keys(ERRORS).every((k) => ERRORS[k].length === 2 && ERRORS[k].every((s) => s.length > 10)), "every error code has a title and a recovery line");
  ok(describeError(Object.assign(new Error("x"), { code: "lost" })).title === ERRORS.lost[0], "describeError maps an fm1ota.js code");
  ok(describeError(new Error("boom")).code === "failed" && describeError(null).code === "failed", "describeError falls back to 'failed'");
  const pkg = { product: "FM-1_900" };
  ok(preflight(pkg, parseId("FM-1_015")).ok && preflight(pkg, parseId("FM-1_015")).action === "install", "preflight: FM-1 running firmware -> install");
  ok(preflight(pkg, parseId("ota-FM-1_900")).action === "resume", "preflight: ORBIT loader -> resume");
  ok(preflight(pkg, parseId("ota-FM-1_015")).code === "foreign", "preflight: another firmware's loader -> foreign");
  ok(preflight(pkg, parseId("XY-9_001")).code === "model", "preflight: another model -> model");
  ok(!preflight(null, parseId("FM-1_015")).ok && !preflight(pkg, null).ok, "preflight: nothing without both package and device");
  ok(progressOf("write", 8, 8192).value === 50 && progressOf("write", 1000, 8192).value === 99, "progress: write percent from reads, capped below 100");
  ok([0, undefined, NaN, -1].every((len) => progressOf("write", 3, len).value === null && /\(3 reads\)/.test(progressOf("write", 3, len).text)),
    "progress: an empty or unknown image length never divides, stays indeterminate");
  ok(progressOf("done", "FM-1_900").value === 100 && progressOf("reboot").value === null && progressOf("loader").value === null, "progress: 100 only on done, indeterminate while waiting");
}

async function packageFirst() {
  ok(new Set(PAGE_TAGS.map((tag) => tag.id)).size === PAGE_TAGS.length, "page IDs are unique so mount selects the intended controls");
  const p = await page({ routes: released });
  ok(!p.ui.file && !/type="file"/.test(PAGE), "latest firmware flow has no file picker");
  ok(p.ui.install.disabled && !p.ui.connect.disabled && p.ui.progress.hidden && p.ui.error.hidden, "initial: Install disabled, Connect enabled, no progress, no error");
  ok(p.ui.release.hidden && p.ctl.state.manifest.version === MANIFEST.version && p.ctl.state.pkg.sha256 === MANIFEST.sha256, "latest release automatically checked; retry hidden after success");
  ok(p.net.log.length === 2 && p.midi.calls.length === 0 && FakeUpdater.instances.length === 0, "page load downloads and validates the latest firmware without MIDI permission");

  await download(p, makePackage({ damage: true }));
  ok(!p.ui.error.hidden && p.ui.error.getAttribute("data-code") === "package" && /damaged package \(file CRC\)/.test(p.ui.errorDetail.textContent),
    "damaged file: rejected with the fm1pkg.js reason");
  ok(p.ui.install.disabled && p.ctl.state.pkg === null && p.ui.pkgProduct.textContent === "\u2014", "damaged file: no package kept, Install disabled");
  await download(p, makePackage({ marker: false }));
  ok(/no Felucca update loader/.test(p.ui.errorDetail.textContent), "file without the loader marker: rejected");
  await download(p, new Uint8Array(100));
  ok(/too short/.test(p.ui.errorDetail.textContent), "file that is not a package: rejected");
  ok(p.midi.calls.length === 0 && FakeUpdater.instances.length === 0, "rejected packages never touch MIDI");

  await download(p);
  ok(p.ui.error.hidden && p.ui.pkgProduct.textContent === "FM-1_900" && p.ui.pkgSize.textContent === "8,212 bytes" && p.ui.pkgSha.textContent === hex(VALID),
    "valid file: identity, size and SHA-256 shown");
  ok(p.ctl.state.pkg.sha256 === MANIFEST.sha256 && p.ui.install.disabled && /Press Connect/.test(p.ui.status.textContent),
    "valid file: Install still disabled until a device is identified");
  return p;
}

async function connectAndInstall(p) {
  const pending = p.ui.connect.click();
  ok(p.midi.calls.length === 1 && p.midi.calls[0].sysex === true && Object.keys(p.midi.calls[0]).length === 1,
    "Connect: requestMIDIAccess({ sysex: true }) called synchronously inside the click");
  ok(p.ui.connect.disabled && p.ui.connect.textContent === "Connecting\u2026" && p.ui.connect.getAttribute("aria-busy") === "true", "Connect: working state");
  await pending;
  const up = FakeUpdater.instances.at(-1);
  ok(FakeUpdater.instances.length === 1 && up.access.script.identity === "FM-1_015" && eq(up.calls.map(String), ["find,any", "close"]),
    "Connect: one Updater on the granted access, one identity read, link closed");
  ok(p.ui.device.textContent === "FM-1_015: official FM-1 firmware" && !p.ui.connect.disabled && p.ui.connect.textContent === "Connect", "Connect: identity shown, button idle again");
  ok(!p.ui.install.disabled && p.ui.install.textContent === "Install" && /^Ready\./.test(p.ui.status.textContent), "preflight passed: Install enabled");

  const seen = [];
  up.script.steps = [["start", "FM-1_015"], ["verify", 1], ["verify", 4], ["loader"], ["write", 4], ["write", 8], ["write", 16], ["reboot"], ["done", "FM-1_900"]];
  up.script.onStep = (kind) => seen.push([kind, p.ui.status.textContent, p.ui.progress.getAttribute("value"),
    p.ui.install.disabled && p.ui.connect.disabled && p.ui.release.disabled, p.ui.install.textContent]);
  await p.ui.install.click();
  ok(eq(up.calls.slice(2).map(String), ["install,FM-1_900,8192"]), "Install: Updater.install(image, product) with the logical image");
  ok(seen.every((s) => s[3] && s[4] === "Installing\u2026"), "during the write every conflicting control is disabled, Install shows its working state");
  ok(seen[0][1] === "FM-1_015 answered; sending the update command." && seen[0][2] === null, "start: device named, progress indeterminate");
  ok(/Step 1 of 2.*\(4 reads\)/.test(seen[2][1]) && seen[3][1].startsWith("Step 1 done") && seen[3][2] === null, "verify/loader phases are told apart");
  ok(seen[5][1] === "Step 2 of 2: writing the firmware, 50%. Keep the FM-1 connected." && seen[5][2] === "50", "write: 8 reads of 512 on 8192 bytes -> 50%");
  ok(seen[7][1].startsWith("Write complete. Waiting for the FM-1 to restart") && seen[7][2] === null, "reboot: progress indeterminate, not declared done");
  ok(p.ui.status.textContent === "Installed. The FM-1 reports FM-1_900." && p.ui.progress.getAttribute("value") === "100" && p.ui.error.hidden, "done: success with the returned identity");
  ok(p.ui.install.disabled && !p.ui.connect.disabled && /Installed: the FM-1 reports FM-1_900\. Press Connect/.test(p.ui.device.textContent),
    "after success: Install needs a fresh preflight, Connect is back");
}

async function installErrors() {
  for (const [code, detail, expectText] of [["mismatch", "FM-1_000", /not a successful install/], ["lost", "", /resumes the write/], ["noloader", "", /stays in update mode/], ["noreturn", "", /Power-cycle/]]) {
    const p = await page({ routes: released });
    await download(p, VALID);
    await p.ui.connect.click();
    const up = FakeUpdater.instances.at(-1);
    up.script.steps = [["start", "FM-1_015"], ["verify", 3], ["loader"], ["write", 16], ["reboot"]];
    up.script.error = { code, detail, message: `fm1ota.js: ${code} ${detail}`.trim() };
    await p.ui.install.click();
    ok(!p.ui.error.hidden && p.ui.error.getAttribute("data-code") === code && p.ui.errorTitle.textContent === ERRORS[code][0] && expectText.test(p.ui.errorRecovery.textContent),
      `${code}: title and recovery text`);
    ok(!/^Installed\./.test(p.ui.status.textContent) && p.ui.progress.getAttribute("value") === null && p.ui.install.disabled && !p.ui.connect.disabled,
      `${code}: no success, progress kept indeterminate, Install closed until Connect`);
    if (code === "mismatch") ok(/FM-1_000/.test(p.ui.errorDetail.textContent) && /Unknown after the error/.test(p.ui.device.textContent), "mismatch: the reported identity is shown, device unknown");
  }
}

async function connectErrors() {
  let p = await page({ midi: midiGranted({ identity: null }), routes: released });
  await download(p, VALID);
  await p.ui.connect.click();
  ok(p.ui.error.getAttribute("data-code") === "notfound" && p.ui.device.textContent === "No FM-1 answered." && p.ui.install.disabled, "no device answers: notfound, Install disabled");

  p = await page({ midi: midiRejected("SecurityError"), routes: released });
  await download(p, VALID);
  const before = FakeUpdater.instances.length;
  await p.ui.connect.click();
  ok(p.midi.calls.length === 1 && p.ui.error.getAttribute("data-code") === "denied" && FakeUpdater.instances.length === before && p.ui.device.textContent === "Not connected",
    "permission denied: 'denied', no Updater, device not connected");
  ok(!p.ui.connect.disabled && p.ui.install.disabled, "permission denied: Connect usable again, Install disabled");
  p = await page({ midi: midiRejected("NotAllowedError"), routes: released });
  await p.ui.connect.click();
  ok(p.ui.error.getAttribute("data-code") === "denied", "NotAllowedError is a denial too");

  p = await page({ midi: midiMissing(), routes: released });
  await p.ui.connect.click();
  ok(p.ui.error.getAttribute("data-code") === "unsupported" && !p.ui.connect.disabled && !p.ctl.state.busy, "no Web MIDI: 'unsupported', page not stuck busy");

  p = await page({ midi: midiGranted({ identity: "XY-9_001" }), routes: released });
  await download(p, VALID);
  await p.ui.connect.click();
  ok(p.ui.error.getAttribute("data-code") === "model" && /the device is XY-9_001, the package is for FM-1_900/.test(p.ui.errorDetail.textContent) && p.ui.install.disabled,
    "another model: 'model', Install disabled");
  ok(/blocked/.test(p.ui.status.textContent), "another model: status says Install is blocked");
  await p.ctl.install();
  ok(!FakeUpdater.instances.at(-1).calls.some((c) => c[0] === "install" || c[0] === "resume"), "another model: a forced install() sends nothing");

  p = await page({ midi: midiGranted({ identity: "ota-FM-1_015" }), routes: released });
  await download(p, VALID);
  await p.ui.connect.click();
  ok(p.ui.error.getAttribute("data-code") === "foreign" && p.ui.install.disabled && /another firmware/.test(p.ui.device.textContent), "stock update loader: 'foreign', Install disabled");

  p = await page({ midi: midiGranted({ identity: "FM-1_015" }), routes: released });
  await download(p, VALID);
  const updaters = FakeUpdater.instances.length;
  await p.ctl.install();
  ok(FakeUpdater.instances.length === updaters && p.midi.calls.length === 0 && p.ui.install.disabled, "install() before Connect: no Updater, no permission, Install stays disabled");
}

async function resumeInLoader() {
  const p = await page({ midi: midiGranted({ identity: "ota-FM-1_900" }), routes: released });
  await download(p, VALID);
  await p.ui.connect.click();
  ok(p.ui.error.hidden && !p.ui.install.disabled && p.ui.install.textContent === "Resume install" && /update mode/.test(p.ui.status.textContent),
    "ORBIT loader found: Resume install offered");
  const up = FakeUpdater.instances.at(-1);
  up.script.steps = [["write", 8], ["reboot"], ["done", "FM-1_900"]];
  await p.ui.install.click();
  const resume = up.calls.find((c) => c[0] === "resume");
  ok(resume && resume[1].product === "FM-1_900" && eq(up.calls.slice(2, 4).map(String), ["find,filtered", "close"]),
    "Resume: the loader is read again, then Updater.resume(image, step, { product })");
  ok(p.ui.status.textContent === "Installed. The FM-1 reports FM-1_900." && p.ui.progress.getAttribute("value") === "100", "Resume: success only with the returned identity");

  const q = await page({ midi: midiGranted({ identity: "ota-FM-1_900" }), routes: released });
  await download(q, VALID);
  await q.ui.connect.click();
  const up2 = FakeUpdater.instances.at(-1);
  up2.script.identity = "ota-FM-1_015";
  await q.ui.install.click();
  ok(q.ui.error.getAttribute("data-code") === "foreign" && !up2.calls.some((c) => c[0] === "resume"), "loader changed to another firmware before the write: refused, nothing sent");

  const r = await page({ midi: midiGranted({ identity: "ota-FM-1_900" }), routes: released });
  await download(r, VALID);
  await r.ui.connect.click();
  FakeUpdater.instances.at(-1).script.resumed = false;
  await r.ui.install.click();
  ok(r.ui.error.getAttribute("data-code") === "stopped" && !/^Installed/.test(r.ui.status.textContent), "resume that does not finish: 'stopped', no success");
}

async function releasePackage() {
  let p = await page({ routes: released });
  ok(p.ui.error.hidden && p.ctl.state.pkg.sha256 === MANIFEST.sha256 && p.ui.pkgProduct.textContent === "FM-1_900" && p.ui.pkgSha.textContent === MANIFEST.sha256,
    "release package: fetched from this origin and validated like a file");
  ok(p.net.log[1] === `${SITE}firmware/orbit-0.6.0.fwsc` && p.midi.calls.length === 0, "release package: same-origin URL, no MIDI permission");

  p = await page({ routes: { ...released, [`${SITE}firmware/orbit-0.6.0.fwsc`]: body(makePackage({ size: 0x2000 + 0x100 })) } });
  await p.ui.release.click();
  ok(p.ui.error.getAttribute("data-code") === "release" && /does not match the manifest/.test(p.ui.errorDetail.textContent) && p.ctl.state.pkg === null,
    "release package that differs from the manifest: rejected before validation");
  p = await page({ routes: { ...released, [`${SITE}manifest.json`]: json({ ...MANIFEST, product: "FM-1_015" }) } });
  await p.ui.release.click();
  ok(p.ui.error.getAttribute("data-code") === "release" && /differs from the manifest/.test(p.ui.errorDetail.textContent), "manifest identity that differs from the package: rejected");

  p = await page({ routes: { ...released, [`${SITE}manifest.json`]: json({ ...MANIFEST, package: "https://github.com/dspaudio/orbit/releases/download/v0.6.0/orbit.fwsc" }) } });
  ok(!p.ui.release.disabled && p.ui.connect.disabled && !p.ctl.state.pkg && p.ui.error.getAttribute("data-code") === "release", "off-origin manifest: no firmware or MIDI, retry available");
  p = await page({ routes: { ...released, [`${SITE}manifest.json`]: json({ ...MANIFEST, sha256: "nope" }) } });
  ok(!p.ui.release.disabled && p.ui.connect.disabled && !p.ctl.state.pkg, "invalid manifest digest: download blocked, retry available");
  p = await page({ routes: {} });
  ok(!p.ui.release.disabled && p.ui.connect.disabled && p.ui.install.disabled && !p.ctl.state.pkg, "missing manifest: retry available, MIDI and install blocked");

  p = await page({ routes: released });
  await download(p, VALID);
  await p.ui.connect.click();
  ok(!p.ui.install.disabled, "package then device: Install enabled");
  await download(p, makePackage({ damage: true }));
  ok(p.ui.install.disabled && p.ui.error.getAttribute("data-code") === "package", "switching to a bad package closes Install");
  Object.assign(p.net.routes, released);
  await p.ui.release.click();
  ok(!p.ui.install.disabled && p.ui.error.hidden && p.ctl.state.device.text === "FM-1_015", "retrying the repaired release reopens Install with the known device");
}

// Explicit gates hold operations open without sleeps or polling.
const deferred = () => {
  let resolve;
  const promise = new Promise((done) => { resolve = done; });
  return { promise, resolve };
};
const RELEASE_090 = { ...MANIFEST, version: "0.9.0", package: "firmware/orbit-0.9.0.fwsc" };
const BLE_BYTES = makePackage({ size: 0x2100 });
const RELEASE_BLE = { ...MANIFEST, version: "0.9.B", package: "firmware/orbit-0.9.B.fwsc", size: BLE_BYTES.length, sha256: hex(BLE_BYTES) };
const VARIANTS = { ...RELEASE_BLE, variants: [RELEASE_090, RELEASE_BLE] };
const variantRoutes = {
  [`${SITE}manifest.json`]: json(VARIANTS),
  [`${SITE}${RELEASE_090.package}`]: body(VALID),
  [`${SITE}${RELEASE_BLE.package}`]: body(BLE_BYTES),
};
async function selectVersion(p, version) {
  p.ui.version.value = version;
  await p.ui.version.dispatch("change");
}

async function releaseVariants() {
  const base = new URL(`${SITE}manifest.json`);
  ok(parseManifest(MANIFEST, base).variants.length === 1, "single release: one default version");
  ok(parseManifest(VARIANTS, base).variants.length === 2, "variants: numeric and Bluetooth versions accepted");
  for (const variants of [[], [RELEASE_090], [RELEASE_BLE, RELEASE_BLE], [{ version: "0.9.B" }], "invalid"]) {
    let bad;
    try { parseManifest({ ...VARIANTS, variants }, base); } catch (e) { bad = e; }
    ok(bad?.code === "release", `invalid variant contract rejected: ${JSON.stringify(variants)}`);
  }
  const single = await page({ routes: released });
  ok(single.ui.version.disabled && single.ui.version.value === MANIFEST.version && single.ui.version.children.length === 1,
    "single release: selector shows the default and stays disabled");
  const p = await page({ routes: variantRoutes });
  ok(p.ui.version.value === "0.9.B" && !p.ui.version.disabled && eq(p.ui.version.children.map((o) => o.value), ["0.9.0", "0.9.B"]),
    "variants: selector lists versions, defaults to the root rather than array order");
  ok(eq(p.net.log, [`${SITE}manifest.json`, `${SITE}${RELEASE_BLE.package}`]),
    "variants: initial download checks only the root release");
  for (const [release, bytes] of [[RELEASE_090, VALID], [RELEASE_BLE, BLE_BYTES]]) {
    const before = p.net.log.length;
    await selectVersion(p, release.version);
    ok(eq(p.net.log.slice(before), [`${SITE}${release.package}`]) && p.ctl.state.manifest.version === release.version,
      `switch to ${release.version}: only the selected release downloaded`);
    ok(p.ui.pkgSha.textContent === hex(bytes) && p.ui.pkgSize.textContent === `${new Intl.NumberFormat("en-US").format(bytes.length)} bytes`
      && p.ui.pkgSource.textContent.endsWith(release.version) && p.ui.releaseNote.textContent.includes(release.version),
      `switch to ${release.version}: version, source, size and hash updated`);
    await p.ui.connect.click();
    const up = FakeUpdater.instances.at(-1);
    up.script.steps = [["done", "FM-1_900"]];
    await p.ui.install.click();
    ok(eq(up.image, validatePackage(bytes).image) && up.calls.some((c) => c[0] === "install" && c[1] === "FM-1_900"),
      `switch to ${release.version}: selected bytes installed despite identical FM-1_900 identity`);
  }
  p.net.routes[`${SITE}${RELEASE_090.package}`] = missing;
  await selectVersion(p, "0.9.0");
  ok(!p.ctl.state.done && p.ui.progress.hidden && p.ui.progress.getAttribute("value") === null
    && p.ctl.state.pkg === null && p.ctl.state.pkgs.length === 0 && p.ui.pkgSha.textContent === "\u2014",
    "failed switch: previous package, success and progress cleared");
  const up = FakeUpdater.instances.at(-1), before = up.calls.length;
  await p.ctl.install();
  ok(p.ui.install.disabled && p.ui.connect.disabled && !p.ui.release.hidden && up.calls.length === before,
    "failed switch: forced install cannot send stale bytes");
  p.net.routes[`${SITE}${RELEASE_090.package}`] = body(VALID);
  await p.ui.release.click();
  ok(p.ctl.state.manifest.version === "0.9.0" && p.ctl.state.pkg.sha256 === hex(VALID),
    "retry: selected version retained instead of returning to the root");
  ok(p.net.options.every((opts) => opts.cache === "no-store"), "all manifest and variant downloads bypass cache");
}

async function busyVariants() {
  const p = await page({ routes: variantRoutes });
  const download = deferred(), entered = deferred();
  p.net.routes[`${SITE}${RELEASE_090.package}`] = {
    ok: true, arrayBuffer: () => { entered.resolve(); return download.promise; },
  };
  p.ui.version.value = "0.9.0";
  const downloading = p.ui.version.dispatch("change");
  await entered.promise;
  const before = p.net.log.length;
  await selectVersion(p, "0.9.B");
  ok(p.ui.version.disabled && p.ui.version.value === "0.9.0" && p.net.log.length === before && p.ctl.state.pkg === null,
    "download busy: forced selection is restored, no concurrent fetch or stale package");
  download.resolve(VALID.slice().buffer);
  await downloading;
  ok(p.ctl.state.manifest.version === "0.9.0" && !p.ui.version.disabled, "download complete: selected version retained, selector unlocked");

  const permission = deferred();
  const q = await page({ routes: variantRoutes, midi: { request: () => permission.promise } });
  const connecting = q.ui.connect.click();
  await selectVersion(q, "0.9.0");
  ok(q.ui.version.disabled && q.ui.version.value === "0.9.B" && q.net.log.length === 2,
    "connect busy: version cannot change while MIDI permission is pending");
  permission.resolve({ script: { identity: "FM-1_015" } });
  await connecting;
  const writing = deferred(), started = deferred();
  const up = FakeUpdater.instances.at(-1);
  up.install = async (image) => { up.image = image.slice(); started.resolve(); await writing.promise; };
  const installing = q.ui.install.click();
  await started.promise;
  await selectVersion(q, "0.9.0");
  ok(q.ui.version.disabled && q.ui.version.value === "0.9.B" && q.net.log.length === 2 && eq(up.image, validatePackage(BLE_BYTES).image),
    "write busy: version and selected bytes cannot change");
  writing.resolve();
  await installing;
  ok(!q.ui.version.disabled, "write complete: selector unlocked");
}

async function defaultFetchOptions() {
  const original = globalThis.fetch, net = fetchFor(variantRoutes);
  try {
    globalThis.fetch = net.fetch;
    const ctl = mount(new FakeDocument());
    await ctl.ready;
    ok(net.options.length === 2 && net.options.every((opts) => opts?.cache === "no-store"),
      "mount default fetch wrapper forwards cache options");
  } finally {
    globalThis.fetch = original;
  }
}

/* ------------------------------------------- the real Updater against the simulated FM-1 (test_web.mjs) --- */
const HS = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
class FakeFM1 {
  constructor(image, { identity = "FM-1_015", name = "FM-1", loader = "ota-FM-1_900", finalIdentity = "FM-1_900" } = {}) {
    this.image = image; this.served = 0; this.bad = 0; this.upgrades = 0;
    this.loader = loader; this.finalIdentity = finalIdentity;
    this.access = { inputs: new Map(), outputs: new Map() };
    this.boot(identity, name);
  }
  boot(identity, name) {
    this.identity = identity; this.waiting = null; this.queue = [];
    for (const m of [this.access.inputs, this.access.outputs]) { for (const p of m.values()) p.state = "disconnected"; m.clear(); }
    const id = Math.random().toString(36).slice(2);
    this.input = { id: "i" + id, name, state: "connected", onmidimessage: null, open: async () => {} };
    this.output = { id: "o" + id, name, state: "connected", open: async () => {}, send: (d) => {
      if (this.output.state !== "connected") throw new Error("InvalidStateError");
      setTimeout(() => this.rx(Array.from(d)), 1);
    } };
    this.access.inputs.set(this.input.id, this.input);
    this.access.outputs.set(this.output.id, this.output);
  }
  tx(bytes) { const i = this.input; setTimeout(() => { if (i.state === "connected" && i.onmidimessage) i.onmidimessage({ data: Uint8Array.from(bytes) }); }, 1); }
  rx(d) {
    if (eq(d, HS)) {
      const t = [...new TextEncoder().encode(this.identity)];
      this.tx([0xF0, ...pack7([0, 0x59, 0x11, 0, 0, 0, ...t, ...new Array(28 - t.length).fill(0)]), 0xF7]);
    } else if (eq(d, UPGRADE)) {
      this.upgrades++;
      this.queue = this.identity.startsWith("ota-")
        ? [...Array.from({ length: 6 }, (_, k) => [k * 512, 512]), [0xF0000000, 8]]
        : [[0, 64], [0x40, 160], [0x1000, 512], [0xE0000000, 8]];
      this.next();
    } else if (this.waiting) {
      const u = unpack7(d.slice(1, -1));
      const [addr, len] = this.waiting;
      const got = u.slice(14, 14 + (addr >= 0xE0000000 ? 8 : len));
      const want = addr >= 0xE0000000 ? [...new TextEncoder().encode("success"), 0] : Array.from(this.image.subarray(addr, addr + len));
      if (!eq(got, want)) this.bad++;
      this.waiting = null;
      this.served++;
      if (addr === 0xE0000000) setTimeout(() => this.boot(this.loader, "Felucca Update"), 300);
      else if (addr === 0xF0000000) setTimeout(() => this.boot(this.finalIdentity, "Felucca"), 300);
      else this.next();
    }
  }
  next() {
    const r = this.queue.shift();
    if (!r) return;
    this.waiting = r;
    const [addr, len] = r;
    const u = [0, 0x59, 0x30, 0, 0, 0, 0, addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF, len & 0xFF, len >> 8, 0];
    let s = 0;
    for (let i = 6; i < 14; i++) s += u[i];
    u.push(~s & 0xFF);
    this.tx([0xF0, ...pack7(u), 0xF7]);
  }
}

async function realUpdater() {
  const image = validatePackage(VALID).image;
  for (const name of ["SLOOP", "Clipper"]) {
    const namedDevice = new FakeFM1(image);
    namedDevice.boot("FM-1_900", name);
    const found = await new Updater(namedDevice.access).find();
    ok(found?.id.text === "FM-1_900" && namedDevice.upgrades === 0,
      `real Updater: ${name} port found without an update`);
    found?.link.close();
  }
  const device = new FakeFM1(image);
  const calls = [];
  const doc = new FakeDocument();
  const ctl = mount(doc, { requestMIDIAccess: (o) => { calls.push(o); return Promise.resolve(device.access); }, fetch: fetchFor({ ...released }).fetch });
  await ctl.ready;
  ok(device.upgrades === 0 && calls.length === 0, "real Updater: a validated package alone sends nothing");
  await doc.ids.connect.click();
  ok(calls.length === 1 && calls[0].sysex === true && doc.ids.device.textContent === "FM-1_015: official FM-1 firmware" && device.upgrades === 0,
    "real Updater: Connect reads the identity over the simulated FM-1 without an update command");
  const t0 = Date.now();
  const before = doc.ids.status.history.length;
  await doc.ids.install.click();
  const texts = doc.ids.status.history.slice(before);
  const secs = ((Date.now() - t0) / 1000).toFixed(1);
  ok(device.upgrades === 2 && device.bad === 0 && device.identity === "FM-1_900", `real Updater: firmware -> loader -> firmware, ${device.served} reads answered correctly (${secs} s)`);
  ok(doc.ids.status.textContent === "Installed. The FM-1 reports FM-1_900." && doc.ids.progress.getAttribute("value") === "100" && doc.ids.error.hidden,
    "real Updater: the page declares success from the returned identity");
  ok(texts.some((t) => /^Step 1 of 2/.test(t)) && texts.some((t) => /^Step 2 of 2: writing the firmware, \d+%/.test(t)) && texts.some((t) => /^Write complete/.test(t)),
    "real Updater: staging, write and reboot phases were shown in order");
}

/* ------------------------------------------- the M-Vave SMK-37 keyboards: a release with their packages --- */
const SMK_PKGS = { "SMK-37 Pro_900": makePackage({ product: "SMK-37 Pro_900" }), "SMK-37 Elite_900": makePackage({ product: "SMK-37 Elite_900" }),
  "MKE-P37_900": makePackage({ product: "MKE-P37_900" }) };
const SMK_NAMES = { "SMK-37 Pro_900": "orbit-smk37", "SMK-37 Elite_900": "orbit-smk37-elite", "MKE-P37_900": "orbit-smk37-mke" };
const MULTI = { ...MANIFEST, packages: [{ product: "FM-1_900", package: MANIFEST.package, size: VALID.length, sha256: hex(VALID) },
  ...Object.entries(SMK_PKGS).map(([product, raw]) => ({ product, package: `firmware/${SMK_NAMES[product]}-0.6.0.fwsc`, size: raw.length, sha256: hex(raw) }))] };
const releasedAll = { ...released, [`${SITE}manifest.json`]: json(MULTI),
  ...Object.fromEntries(Object.entries(SMK_PKGS).map(([product, raw]) => [`${SITE}firmware/${SMK_NAMES[product]}-0.6.0.fwsc`, body(raw)])) };

async function smkKeyboards() {
  const smk = { product: "SMK-37 Pro_900" }, fm1 = { product: "FM-1_900" };
  ok(preflight(smk, parseId("SMK-37 Pro_016")).ok && !preflight(smk, parseId("SMK-37 Pro_016")).anyModel, "preflight: SMK-37 Pro package on an SMK-37 Pro -> install");
  ok(preflight(smk, parseId("SMK-37 Elite_016")).ok && preflight(smk, parseId("STARRYKEY-37 PLAY_016")).anyModel,
    "preflight: another model of the same keyboard -> install (anyModel)");
  ok(preflight(smk, parseId("FM-1_015")).code === "model" && preflight(fm1, parseId("SMK-37 Pro_016")).code === "model",
    "preflight: FM-1 and SMK-37 never mix ('model')");
  ok(preflight(smk, parseId("SMK-37_931")).code === "other" && preflight(fm1, parseId("SMK-37_941")).code === "other",
    "preflight: a firmware with its own updater (SMK-37_9NN) -> 'other'");
  ok(preflight(smk, parseId("ota-SMK-37_931")).code === "foreign", "preflight: its updater -> 'foreign'");
  ok(preflight(smk, parseId("ota-SMK-37 Elite_900")).action === "resume" && preflight(smk, parseId("ota-MKE-P37_900")).anyModel,
    "preflight: ORBIT's loader under an SMK model's name -> resume");
  ok(preflight(fm1, parseId("ota-FM-1_900")).anyModel === false, "preflight: FM-1 unchanged (no anyModel)");
  ok(/official M-Vave firmware/.test(describeDevice(parseId("SMK-37 Elite_016"))) && /ORBIT \/ SLOOP/.test(describeDevice(parseId("MKE-P37_900")))
    && /own updater/.test(describeDevice(parseId("SMK-37_931"))) && describeDevice(parseId("FM-1_015")) === "FM-1_015: official FM-1 firmware",
    "describeDevice: SMK-37 models named, the FM-1 text unchanged");
  const pk = MULTI.packages.map((e) => ({ product: e.product }));
  ok(packageFor(pk, parseId("SMK-37 Elite_016")).product === "SMK-37 Elite_900" && packageFor(pk, parseId("ota-MKE-P37_900")).product === "MKE-P37_900"
    && packageFor(pk, parseId("STARRYKEY-37 PLAY_016")).product === "SMK-37 Pro_900" && packageFor(pk, parseId("FM-1_015")).product === "FM-1_900"
    && packageFor(pk, null).product === "FM-1_900", "packageFor: the model's own package, else the keyboard's first, else the first");
  const m = parseManifest(MULTI, new URL(`${SITE}manifest.json`));
  ok(m.packages.length === 4 && m.product === "FM-1_900", "manifest with packages: every package parsed, the FM-1 fields unchanged");
  let bad = null;
  try { parseManifest({ ...MULTI, packages: [MULTI.packages[0], MULTI.packages[0]] }, new URL(`${SITE}manifest.json`)); } catch (e) { bad = e; }
  ok(bad && bad.code === "release", "manifest with one identity twice: refused");
  bad = null;
  try { parseManifest({ ...MULTI, packages: [{ ...MULTI.packages[1], package: "https://elsewhere.test/x.fwsc" }] }, new URL(`${SITE}manifest.json`)); } catch (e) { bad = e; }
  ok(bad && bad.code === "release", "manifest package off this origin: refused");

  let p = await page({ midi: midiGranted({ identity: "SMK-37 Elite_016" }), routes: releasedAll });
  ok(p.net.log.length === 5 && p.ctl.state.pkgs.length === 4 && p.ctl.state.pkg.product === "FM-1_900" && /FM-1_900, SMK-37 Pro_900/.test(p.ui.releaseNote.textContent),
    "release with SMK packages: all four downloaded and checked, the FM-1's shown first");
  await p.ui.connect.click();
  const up = FakeUpdater.instances.at(-1);
  ok(p.ctl.state.pkg.product === "SMK-37 Elite_900" && p.ui.pkgProduct.textContent === "SMK-37 Elite_900" && !p.ui.install.disabled
    && /official M-Vave firmware/.test(p.ui.device.textContent), "SMK-37 Elite connected: its own package chosen and shown, Install enabled");
  await p.ui.install.click();
  ok(eq(up.calls.slice(2).map(String), ["install,SMK-37 Elite_900,8192"]) && up.anyModel === false, "Install: the Elite package, no other model needed");

  p = await page({ midi: midiGranted({ identity: "STARRYKEY-37 PLAY_016" }), routes: releasedAll });
  await p.ui.connect.click();
  const up2 = FakeUpdater.instances.at(-1);
  await p.ui.install.click();
  ok(p.ctl.state.done === true && up2.calls.some((c) => c[0] === "install" && c[1] === "SMK-37 Pro_900") && up2.anyModel === true,
    "Starrykey 37 Play: the SMK-37 Pro package, written with anyModel");

  p = await page({ midi: midiGranted({ identity: "ota-SMK-37 Pro_900" }), routes: releasedAll });
  await p.ui.connect.click();
  const up3 = FakeUpdater.instances.at(-1);
  await p.ui.install.click();
  const resume = up3.calls.find((c) => c[0] === "resume");
  ok(resume && resume[1].product === "SMK-37 Pro_900" && resume[1].anyModel === false, "SMK loader: resume with the SMK-37 Pro package's identity");

  p = await page({ midi: midiGranted({ identity: "SMK-37_931" }), routes: releasedAll });
  await p.ui.connect.click();
  ok(p.ui.error.getAttribute("data-code") === "other" && p.ui.install.disabled && p.ui.errorTitle.textContent === ERRORS.other[0],
    "SMK-37_9NN firmware: 'other', Install disabled");
  await p.ctl.install();
  ok(!FakeUpdater.instances.at(-1).calls.some((c) => c[0] === "install" || c[0] === "resume"), "SMK-37_9NN firmware: a forced install() sends nothing");

  p = await page({ midi: midiGranted({ identity: "SMK-37 Pro_016" }), routes: released });
  await p.ui.connect.click();
  ok(p.ui.error.getAttribute("data-code") === "model" && p.ui.install.disabled, "FM-1-only release on an SMK-37: 'model', nothing to install");

  const image = validatePackage(SMK_PKGS["SMK-37 Pro_900"]).image;
  const device = new FakeFM1(image, { identity: "SMK-37 Elite_016", name: "SMK-37 Elite", loader: "ota-SMK-37 Pro_900", finalIdentity: "SMK-37 Pro_900" });
  const doc = new FakeDocument();
  const routes = { ...releasedAll, [`${SITE}manifest.json`]: json({ ...MULTI, packages: [MULTI.packages[0], MULTI.packages[1]] }) };
  const ctl = mount(doc, { requestMIDIAccess: () => Promise.resolve(device.access), fetch: fetchFor(routes).fetch });
  await ctl.ready;
  await doc.ids.connect.click();
  await doc.ids.install.click();
  ok(device.upgrades === 2 && device.bad === 0 && device.identity === "SMK-37 Pro_900" && doc.ids.status.textContent === "Installed. The FM-1 reports SMK-37 Pro_900.",
    "real Updater: an SMK-37 Elite takes the SMK-37 Pro package (one keyboard), success from the returned identity");
  const other = new FakeFM1(image, { identity: "SMK-37_931", name: "Felucca" });
  const doc2 = new FakeDocument();
  const ctl2 = mount(doc2, { requestMIDIAccess: () => Promise.resolve(other.access), fetch: fetchFor(releasedAll).fetch });
  await ctl2.ready;
  await doc2.ids.connect.click();
  await ctl2.install();
  ok(other.upgrades === 0 && doc2.ids.error.getAttribute("data-code") === "other", "real Updater: an SMK-37_9NN firmware is never sent UPGRADE");
}

const p = await packageFirst();
await pureFunctions();
await connectAndInstall(p);
await installErrors();
await connectErrors();
await resumeInLoader();
await releasePackage();
await releaseVariants();
await busyVariants();
await defaultFetchOptions();
await realUpdater();
await smkKeyboards();

if (failures.length) {
  console.log(`\n${failures.length} failed:\n  ${failures.join("\n  ")}`);
  process.exit(1);
}
console.log("installer tests passed");
