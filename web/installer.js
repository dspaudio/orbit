// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// The ORBIT web installer page: package selection and validation, one explicit Connect
// gesture for Web MIDI with SysEx, the device preflight, then the M-UPGRADE install
// through fm1ota.js. A release may carry packages for the M-Vave SMK-37 keyboards too
// (manifest.json "packages"): the page then writes the one for the connected keyboard. Nothing is sent to the FM-1 before the package passed validation
// and the device answered the identity request; success is declared only when the
// device returns with the package identity. The page is wired by mount(document);
// the tests mount a fake document with injected MIDI, fetch and Updater doubles.

import { Updater, OUR_LOADER, OTHER_SMK, sameKeyboard } from "./fm1ota.js";
import { validatePackage, sha256hex } from "./fm1pkg.js";

const MAXDATA = 512;                                  // the largest read the loader asks for (fm1ota.js)
const IS_OTA = (id) => /^ota-/i.test(id.model);
const modelOf = (id) => id.model.replace(/^ota-/i, "");
const VERSION = /^\d+\.\d+\.\d+(-[0-9A-Za-z.]+)?$/;
const IDENTITY = /^[^_]+_\d+$/;
const number = new Intl.NumberFormat("en-US");
const fail = (code, msg, detail = "") => Object.assign(new Error(msg), { code, detail });

export const RECOVERY_URL = "https://github.com/kurogedelic/FM-1-transporter";

// error codes (fm1ota.js and this page) -> what happened, what to do next
export const ERRORS = {
  unsupported: ["This browser has no Web MIDI.", "Use a Chromium-based desktop browser (Chrome, Edge, Brave) on HTTPS or localhost."],
  denied: ["MIDI access was denied.", "Allow MIDI with SysEx for this site in the browser's site settings, then press Connect again."],
  notfound: ["No FM-1 answered.", "Use a USB data cable, close other apps that hold the FM-1's MIDI port (the editor, a DAW), then press Connect again."],
  model: ["This firmware is for a different device.", "Nothing was sent. Connect the FM-1 and try again."],
  other: ["The keyboard runs a firmware with its own updater.", "Nothing was sent. Return the keyboard to M-Vave's firmware with that firmware's own updater first, then press Connect again."],
  foreign: ["The FM-1 is in another firmware's update mode.", "Nothing was sent. Finish that update with its own updater (M-UPGRADE) first; this page never writes through another loader."],
  stopped: ["The device stopped answering.", "Keep the FM-1 connected, press Connect, then Install again. A device left in update mode resumes the write."],
  lost: ["The USB connection was lost.", "Reconnect the cable, press Connect, then Install again. A device left in update mode resumes the write."],
  noloader: ["The update loader did not appear.", "The FM-1 stays in update mode until the install finishes: unplug and replug the USB cable, press Connect, then Install to resume."],
  noreturn: ["The device did not come back.", "Power-cycle the FM-1, then press Connect to read its identity before trying again."],
  mismatch: ["The device reports a different firmware.", "This is not a successful install. Press Connect to read the identity again; if it shows the recovery firmware, run Install again."],
  badreq: ["The device asked for data outside the package.", "Stop here and check the package; do not retry with this file."],
  package: ["The release package was rejected.", "Nothing was sent. Retry the download; if the problem remains, use the CLI installer linked below."],
  release: ["The latest release could not be loaded.", "Check your connection and press Retry download."],
  failed: ["The install did not finish.", "Keep the FM-1 connected, press Connect to read its identity, then Install again. If the FM-1 no longer starts, see the recovery link below."],
};

export function describeError(err) {
  const code = err && err.code in ERRORS ? err.code : "failed";
  const [title, recovery] = ERRORS[code];
  return { code, title, detail: (err && err.message) || "", recovery };
}

export function describeDevice(id) {
  if (IS_OTA(id)) {
    return OUR_LOADER(id) ? `${id.text}: ORBIT update loader, an interrupted install can be resumed`
      : `${id.text}: update loader of another firmware`;
  }
  if (OTHER_SMK(id)) return `${id.text}: SMK-37 firmware with its own updater`;
  if (id.model !== "FM-1" && sameKeyboard(id.model, "SMK-37 Pro"))
    return `${id.text}: ${id.version >= 900 ? "ORBIT / SLOOP firmware" : id.version < 100 ? "official M-Vave firmware" : "another firmware"}`;
  if (id.model !== "FM-1") return `${id.text}: not an FM-1`;
  return `${id.text}: ${id.version >= 900 ? "ORBIT / SLOOP firmware" : "official FM-1 firmware"}`;
}

// may Install run? the package model must match the device (an SMK-37 package fits every model of that
// keyboard: anyModel), a loader must be ORBIT's own, and a firmware with its own updater is never sent anything
export function preflight(pkg, id) {
  if (!pkg || !id) return { ok: false };
  if (IS_OTA(id) && !OUR_LOADER(id)) {
    return { ok: false, code: "foreign", detail: `the FM-1 is in the update mode of another firmware (${id.text})` };
  }
  if (OTHER_SMK(id)) {
    return { ok: false, code: "other", detail: `the keyboard runs a firmware with its own updater (${id.text})` };
  }
  const dm = modelOf(id), pm = pkg.product.split("_")[0];
  if (dm !== pm && !sameKeyboard(dm, pm)) {
    return { ok: false, code: "model", detail: `the device is ${id.text}, the package is for ${pkg.product}` };
  }
  return { ok: true, action: IS_OTA(id) ? "resume" : "install", anyModel: dm !== pm };
}

// the release package for a device: its own model's, else one of the same keyboard's, else the first
export function packageFor(pkgs, id) {
  if (!pkgs.length || !id) return pkgs[0] || null;
  const dm = modelOf(id);
  return pkgs.find((p) => p.product.split("_")[0] === dm) ||
    pkgs.find((p) => sameKeyboard(dm, p.product.split("_")[0])) || pkgs[0];
}

// value null leaves the <progress> indeterminate
export function progressOf(step, arg, imageLength) {
  switch (step) {
    case "start": return { value: null, text: `${arg} answered; sending the update command.` };
    case "verify": return { value: null, text: `Step 1 of 2: the running firmware is staging the update loader (${arg} reads).` };
    case "loader": return { value: null, text: "Step 1 done. Waiting for the update loader (up to 30 s); the FM-1 restarts by itself." };
    case "write": {
      if (!(imageLength > 0)) return { value: null, text: `Step 2 of 2: writing the firmware (${arg} reads). Keep the FM-1 connected.` };
      const pct = Math.min(99, Math.floor((arg * MAXDATA * 100) / imageLength));
      return { value: pct, text: `Step 2 of 2: writing the firmware, ${pct}%. Keep the FM-1 connected.` };
    }
    case "reboot": return { value: null, text: "Write complete. Waiting for the FM-1 to restart (up to 40 s)." };
    case "done": return { value: 100, text: `Installed. The FM-1 reports ${arg}.` };
    default: return { value: null, text: `${step}` };
  }
}

// manifest.json of a generated site (tools/make_site.py): the release package on this origin; "packages", when
// there, lists every package of the release (the FM-1's first, then the SMK-37 models'), each checked the same way
function parseEntry(e, base) {
  if (!e || typeof e !== "object") throw fail("release", "the manifest has an invalid package entry");
  const { product, size, sha256 } = e;
  if (typeof product !== "string" || !IDENTITY.test(product)) throw fail("release", "the manifest has no valid product identity");
  if (!Number.isInteger(size) || size <= 0) throw fail("release", "the manifest has no valid size");
  if (typeof sha256 !== "string" || !/^[0-9a-f]{64}$/.test(sha256)) throw fail("release", "the manifest has no valid SHA-256");
  if (typeof e.package !== "string") throw fail("release", "the manifest names no package");
  const url = new URL(e.package, base);
  if (url.origin !== base.origin) throw fail("release", `the release package is not on this origin (${url.origin})`);
  return { product, size, sha256, url: url.href };
}

export function parseManifest(json, base) {
  if (!json || typeof json !== "object") throw fail("release", "the manifest is not an object");
  const { version, product, size, sha256, name } = json;
  if (typeof version !== "string" || !VERSION.test(version)) throw fail("release", "the manifest has no valid version");
  if (typeof product !== "string" || !IDENTITY.test(product)) throw fail("release", "the manifest has no valid product identity");
  if (!Number.isInteger(size) || size <= 0) throw fail("release", "the manifest has no valid size");
  if (typeof sha256 !== "string" || !/^[0-9a-f]{64}$/.test(sha256)) throw fail("release", "the manifest has no valid SHA-256");
  if (typeof json.package !== "string") throw fail("release", "the manifest names no package");
  const url = new URL(json.package, base);
  if (url.origin !== base.origin) throw fail("release", `the release package is not on this origin (${url.origin})`);
  const releaseUrl = typeof json.release_url === "string" && /^https:\/\//.test(json.release_url) ? json.release_url : "";
  const packages = Array.isArray(json.packages) ? json.packages.map((e) => parseEntry(e, base)) : [{ product, size, sha256, url: url.href }];
  if (!packages.length || new Set(packages.map((p) => p.product)).size !== packages.length)
    throw fail("release", "the manifest lists no packages, or one identity twice");
  return { name: typeof name === "string" ? name : "ORBIT", version, product, size, sha256, url: url.href, releaseUrl, packages };
}

export function createInstaller(deps) {
  const { ui, requestMIDIAccess, manifestUrl = "manifest.json", baseUrl } = deps;
  const fetchFn = deps.fetch;
  const UpdaterClass = deps.Updater || Updater;
  const validate = deps.validatePackage || validatePackage;
  const digest = deps.sha256hex || sha256hex;
  const state = { pkg: null, pkgs: [], device: null, updater: null, manifest: null, busy: false, done: false };
  const labels = { connect: ui.connect.textContent, install: ui.install.textContent };
  const WORKING = { connect: "Connecting\u2026", install: "Installing\u2026" };

  function setProgress(p) {
    ui.progress.hidden = false;
    if (p.value === null) ui.progress.removeAttribute("value");
    else ui.progress.setAttribute("value", String(p.value));
    ui.status.textContent = p.text;
  }

  function showError(err) {
    const d = describeError(err);
    ui.error.hidden = false;
    ui.error.setAttribute("data-code", d.code);
    ui.errorTitle.textContent = d.title;
    ui.errorDetail.textContent = d.detail;
    ui.errorRecovery.textContent = d.recovery;
  }

  function clearError() {
    ui.error.hidden = true;
    ui.error.removeAttribute("data-code");
    ui.errorTitle.textContent = ui.errorDetail.textContent = ui.errorRecovery.textContent = "";
  }

  function showPackage() {
    const p = state.pkg;
    ui.pkgSource.textContent = p ? p.source : "\u2014";
    ui.pkgProduct.textContent = p ? p.product : "\u2014";
    ui.pkgSize.textContent = p ? `${number.format(p.size)} bytes` : "\u2014";
    ui.pkgSha.textContent = p ? p.sha256 : "\u2014";
  }

  function setBusy(what) {
    state.busy = Boolean(what);
    for (const key of ["connect", "install"]) {
      const working = what === key;
      ui[key].textContent = working ? WORKING[key] : labels[key];
      if (working) ui[key].setAttribute("aria-busy", "true");
      else ui[key].removeAttribute("aria-busy");
    }
    refresh();
  }

  function refresh() {
    const check = preflight(state.pkg, state.device);
    ui.release.disabled = state.busy;
    ui.release.hidden = state.busy || Boolean(state.pkg);
    ui.connect.disabled = state.busy || !state.pkg;
    ui.install.disabled = state.busy || !check.ok;
    if (!state.busy) labels.install = check.ok && check.action === "resume" ? "Resume install" : "Install";
    if (!state.busy) ui.install.textContent = labels.install;
    if (state.done || state.busy) return;
    if (!state.pkg) ui.status.textContent = "The latest firmware has not been checked yet. Retry the download.";
    else if (!state.device) ui.status.textContent = "Latest firmware checked. Press Connect to read the device identity.";
    else if (check.ok) ui.status.textContent = check.action === "resume"
      ? "The FM-1 is in update mode: Resume install writes the firmware again from the start."
      : "Ready. Install replaces the firmware of the connected FM-1 with this package.";
    else ui.status.textContent = "Install is blocked: see the message below.";
  }

  async function useRelease() {
    if (state.busy) return;
    setBusy("download");
    clearError();
    state.pkg = null;
    state.pkgs = [];
    state.manifest = null;
    state.done = false;
    ui.status.textContent = "Downloading and checking the latest firmware...";
    showPackage();
    try {
      const base = new URL(manifestUrl, baseUrl);
      const manifest = await fetchFn(base.href, { cache: "no-store" });
      if (!manifest.ok) throw fail("release", `manifest.json: HTTP ${manifest.status}`);
      const m = parseManifest(await manifest.json(), base);
      state.manifest = m;
      ui.releaseNote.textContent = m.packages.length > 1
        ? `${m.name} ${m.version} for ${m.packages.map((p) => p.product).join(", ")}.`
        : `${m.name} ${m.version} for ${m.product}, ${number.format(m.size)} bytes.`;
      const pkgs = [];
      for (const e of m.packages) {
        const res = await fetchFn(e.url, { cache: "no-store" });
        if (!res.ok) throw fail("release", `${e.url}: HTTP ${res.status}`);
        const bytes = new Uint8Array(await res.arrayBuffer());
        const sha256 = await digest(bytes);
        if (bytes.length !== e.size || sha256 !== e.sha256) {
          throw fail("release", `the downloaded package does not match the manifest (${number.format(bytes.length)} bytes, SHA-256 ${sha256})`);
        }
        let valid;
        try { valid = validate(bytes); } catch (x) { throw fail("package", x.message); }
        if (valid.product !== e.product) {
          throw fail("release", `the package identity ${valid.product} differs from the manifest (${e.product})`);
        }
        pkgs.push({ source: `Latest release ${m.name} ${m.version}`, product: valid.product, image: valid.image, size: bytes.length, sha256 });
      }
      state.pkgs = pkgs;
      state.pkg = packageFor(pkgs, state.device);
    } catch (e) {
      showError(e.code ? e : fail("release", e.message));
    }
    setBusy("");
    showPackage();
  }

  // Connect: the only place that asks for MIDI, synchronously inside the click so the gesture counts
  function connect() {
    if (state.busy || !state.pkg) return Promise.resolve();
    let access;
    try {
      access = requestMIDIAccess({ sysex: true });
    } catch (e) {
      showError(fail("unsupported", e.message));
      return Promise.resolve();
    }
    return identify(access);
  }

  async function identify(accessPromise) {
    state.done = false;
    setBusy("connect");
    clearError();
    state.device = null;
    ui.device.textContent = "Not connected";
    try {
      const access = await accessPromise;
      state.updater = new UpdaterClass(access);
      ui.device.textContent = "Looking for an FM-1\u2026";
      const dev = await state.updater.find();
      if (!dev) {
        ui.device.textContent = "No FM-1 answered.";
        throw fail("notfound", "no MIDI port answered the FM-1 identity request");
      }
      dev.link.close();
      state.device = dev.id;
      state.pkg = packageFor(state.pkgs, dev.id);                    // the release's package for this keyboard
      showPackage();
      ui.device.textContent = describeDevice(dev.id);
      const check = preflight(state.pkg, dev.id);
      if (state.pkg && !check.ok) showError(fail(check.code, check.detail, dev.id.text));
    } catch (e) {
      if (e && (e.name === "SecurityError" || e.name === "NotAllowedError")) showError(fail("denied", e.message));
      else if (e && e.name === "NotSupportedError") showError(fail("unsupported", e.message));
      else showError(e);
      if (!state.device) ui.device.textContent = ui.device.textContent === "Looking for an FM-1\u2026" ? "Not connected" : ui.device.textContent;
    } finally {
      setBusy(null);
    }
  }

  // Install: the running firmware stages the loader, the loader writes the image, the device
  // comes back; a device already in ORBIT's update mode resumes the write instead
  async function install() {
    const check = preflight(state.pkg, state.device);
    if (state.busy || !check.ok || !state.updater) return;
    const { image, product } = state.pkg;
    setBusy("install");
    clearError();
    const step = (kind, arg) => setProgress(progressOf(kind, arg, image.length));
    try {
      if (check.action === "resume") {
        const ota = await state.updater.find(IS_OTA);                 // read the loader again right before writing
        if (!ota) throw fail("notfound", "the update loader is no longer answering; press Connect to read the device identity");
        ota.link.close();
        if (!OUR_LOADER(ota.id)) throw fail("foreign", `the FM-1 is in the update mode of another firmware (${ota.id.text})`, ota.id.text);
        step("write", 0);
        const finished = await state.updater.resume(image, step, { product, anyModel: check.anyModel });
        if (!finished) throw fail("stopped", "the loader stopped before the write finished; nothing new is running yet");
      } else {
        await state.updater.install(image, product, step, check.anyModel);
      }
      state.done = true;
      ui.device.textContent = `Installed: the FM-1 reports ${product}. Press Connect to read it again.`;
    } catch (e) {
      showError(e);
      ui.device.textContent = "Unknown after the error: press Connect to read the device identity.";
    } finally {
      state.device = null;                                           // every new write starts from a fresh preflight
      setBusy(null);
    }
  }

  showPackage();
  refresh();
  const ready = useRelease();
  return { state, ready, useRelease, connect, install };
}

const IDS = ["release", "releaseNote", "pkgSource", "pkgProduct", "pkgSize", "pkgSha", "connect", "device",
  "install", "progress", "status", "error", "errorTitle", "errorDetail", "errorRecovery"];

function requestMIDI(options) {
  if (typeof navigator === "undefined" || typeof navigator.requestMIDIAccess !== "function") {
    throw fail("unsupported", "navigator.requestMIDIAccess is not available in this browser");
  }
  return navigator.requestMIDIAccess(options);
}

// wire the page: overrides inject doubles (tests) for requestMIDIAccess, fetch, Updater, baseUrl
export function mount(doc, overrides = {}) {
  const ui = {};
  for (const id of IDS) {
    const el = doc.getElementById(id);
    if (!el) throw new Error(`installer page: missing #${id}`);
    ui[id] = el;
  }
  const ctl = createInstaller({
    ui,
    manifestUrl: (doc.body && doc.body.dataset && doc.body.dataset.manifest) || "manifest.json",
    baseUrl: doc.baseURI,
    requestMIDIAccess: requestMIDI,
    fetch: (url) => fetch(url),
    ...overrides,
  });
  ui.release.addEventListener("click", () => ctl.useRelease());
  ui.connect.addEventListener("click", () => ctl.connect());
  ui.install.addEventListener("click", () => ctl.install());
  const win = doc.defaultView;
  if (win) win.addEventListener("beforeunload", (e) => { if (ctl.state.busy) e.preventDefault(); });
  return ctl;
}
