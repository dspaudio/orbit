#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""tools/make_site.py against a synthetic package (the one tests/install_test.py builds) and,
when build/orbit.fwsc and build/orbit-ble.fwsc exist, against both real packages.
CI also uses release/orbit.fwsc and release/ble/orbit-ble.fwsc. Run from the repo root:
  python3 tests/site_test.py
Writes only below a temporary directory; build/site is left to the generator's own runs."""
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_install as I  # noqa: E402

WEB = ROOT / "web"
COPIED = ["installer.js", "fm1ota.js", "fm1pkg.js", "editor.html", "fukiai.ttf", "FUKIAI-LICENSE.txt",
          "screens/envelope.svg", "screens/preset.svg", "screens/tape.svg"]
TMP = Path(tempfile.mkdtemp(prefix="orbit-site-"))
failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<72} {'ok' if cond else 'FAIL'}")
    failed += not cond


def package(product="FM-1_900", marker=True, size=0x2000, damage=False):
    img = bytearray((i * 7) & 0xFF for i in range(size))
    img[0x1800:0x1800 + 16] = I.LOADER_MARK if marker else bytes(16)
    data = bytes(img[0x400:])
    entry = bytearray(0x50)
    entry[4:6] = I._crc16(data).to_bytes(2, "little")
    entry[8:12] = (0x400).to_bytes(4, "little")
    entry[12:16] = len(data).to_bytes(4, "little")
    entry[0x40:0x49] = b"flash.bin"
    lst = I._dec(bytes(entry))
    hdr = bytearray(0x40)
    hdr[4:8] = size.to_bytes(4, "little")
    hdr[8:10] = (1).to_bytes(2, "little")
    hdr[2:4] = I._crc16(lst).to_bytes(2, "little")
    hdr[0:2] = I._crc16(bytes(hdr[2:0x40])).to_bytes(2, "little")
    img[0:0x40] = I._dec(bytes(hdr))
    img[0x40:0x90] = lst
    if damage:
        img[0x1000] ^= 1
    raw = bytearray()
    for i in range(I.BLOCKS):
        marker_byte = (ord(product[i]) + i + 1) & 0xFF if i < len(product) else 0x7D
        raw += img[i * I.KEEP:(i + 1) * I.KEEP] + bytes([marker_byte])
    raw += img[I.BLOCKS * I.KEEP:]
    return bytes(raw)


def run(*args):
    result = subprocess.run([sys.executable, str(ROOT / "tools/make_site.py"), *args], cwd=ROOT,
                            capture_output=True, text=True)
    return result.returncode, result.stdout, result.stderr


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tree(out):
    return {path.relative_to(out).as_posix(): sha(path) for path in out.rglob("*") if path.is_file()}


def generated_site():
    raw = package()
    pkg = TMP / "orbit.fwsc"
    pkg.write_bytes(raw)
    out = TMP / "site"
    args = ("--package", str(pkg), "--version", "0.0.1", "--release-url", "https://example.test/releases/tag/v0.0.1")
    rc, stdout, stderr = run(*args, "--out", str(out))
    ok(rc == 0 and "FM-1_900" in stdout and not stderr, "generator exits 0 and names the package identity")
    ok((out / "index.html").read_bytes() == (WEB / "installer.html").read_bytes(), "index.html is web/installer.html byte for byte")
    ok(all((out / name).read_bytes() == (WEB / name).read_bytes() for name in COPIED), "installer modules, editor, font and native screenshots copied unchanged")
    index = (out / "index.html").read_text()
    ok('from "./installer.js"' in index and 'data-manifest="manifest.json"' in index, "index.html loads installer.js and names manifest.json")
    screens = re.findall(r'<img[^>]+src="([^"]+)"', index)
    ok(len(screens) == 3 and all((out / name).is_file() for name in screens),
       "all three native screen references resolve in the generated site")
    imports = {m for text in [(out / "installer.js").read_text()] for m in re.findall(r'from "\./([^"]+)"', text)}
    ok(imports == {"fm1ota.js", "fm1pkg.js"} and all((out / name).exists() for name in imports), "installer.js imports resolve inside the site")
    ok("fukiai.ttf" in (out / "editor.html").read_text(), "the copied editor still names the font shipped beside it")

    manifest = json.loads((out / "manifest.json").read_text())
    ok(manifest == {"name": "ORBIT", "version": "0.0.1", "product": "FM-1_900", "package": "firmware/orbit-0.0.1.fwsc",
                    "size": len(raw), "sha256": hashlib.sha256(raw).hexdigest(),
                    "release_url": "https://example.test/releases/tag/v0.0.1"},
       "manifest: version, identity, path, size and SHA-256 of the actual input")
    ok((out / manifest["package"]).read_bytes() == raw, "the versioned package is the input byte for byte")
    sums = {line[66:]: line[:64] for line in (out / "SHA256SUMS").read_text().splitlines()}
    listed = tree(out)
    del listed["SHA256SUMS"]
    ok(sums == listed and all(line[64:66] == "  " for line in (out / "SHA256SUMS").read_text().splitlines()),
       "SHA256SUMS lists every other file with its digest, in sha256sum format")

    out2 = TMP / "site-again"
    rc2, _, _ = run(*args, "--out", str(out2))
    ok(rc2 == 0 and tree(out) == tree(out2), "two runs from the same inputs give identical files")
    rc3, _, _ = run(*args, "--out", str(out))
    ok(rc3 == 0 and tree(out) == tree(out2), "regenerating over a generated site replaces it cleanly")
    return pkg


def failures(good):
    out = TMP / "bad"
    cases = [
        (TMP / "missing.fwsc", None, "0.0.1", "cannot read", "missing package"),
        (TMP / "damaged.fwsc", package(damage=True), "0.0.1", "damaged package", "damaged package (a file's CRC)"),
        (TMP / "plain.fwsc", package(marker=False), "0.0.1", "no Felucca update loader", "package without the loader"),
        (TMP / "short.fwsc", b"\0" * 100, "0.0.1", "too short", "not a package"),
        (TMP / "other.fwsc", package(product="FM1-900"), "0.0.1", "not an FM-1 package", "package without a valid identity"),
        (good, None, "v0.0.1", "version", "version with a leading v"),
        (good, None, "0.1", "version", "two-part version"),
        (good, None, "0.9.B", "version", "BLE version used as the default"),
    ]
    for path, content, version, message, label in cases:
        if content is not None:
            path.write_bytes(content)
        rc, _, err = run("--package", str(path), "--version", version, "--out", str(out))
        ok(rc == 2 and message in err and not out.exists(), f"{label}: exit 2, nothing written")
    rc, _, err = run("--package", str(good), "--version", "0.0.1", "--out", str(out), "--release-url", "http://example.test")
    ok(rc == 2 and "https" in err and not out.exists(), "release URL that is not https: exit 2")
    occupied = TMP / "occupied"
    occupied.mkdir()
    (occupied / "keep.txt").write_text("mine")
    rc, _, err = run("--package", str(good), "--version", "0.0.1", "--out", str(occupied))
    ok(rc == 2 and "refusing" in err and (occupied / "keep.txt").read_text() == "mine", "an unrelated output directory is never deleted")


def smk_packages(good):
    """the SMK-37 packages of the same release: listed in manifest.json "packages", the FM-1 fields unchanged"""
    paths = [good]
    for name, product in (("orbit-smk37", "SMK-37 Pro_900"), ("orbit-smk37-elite", "SMK-37 Elite_900"), ("orbit-smk37-mke", "MKE-P37_900")):
        path = TMP / f"{name}.fwsc"
        path.write_bytes(package(product=product))
        paths.append(path)
    out = TMP / "site-smk"
    args = [a for p in paths for a in ("--package", str(p))]
    rc, _, err = run(*args, "--version", "0.0.1", "--out", str(out))
    manifest = json.loads((out / "manifest.json").read_text()) if rc == 0 else {}
    entries = manifest.get("packages", [])
    ok(rc == 0 and manifest.get("product") == "FM-1_900" and manifest.get("package") == "firmware/orbit-0.0.1.fwsc"
       and [e["product"] for e in entries] == ["FM-1_900", "SMK-37 Pro_900", "SMK-37 Elite_900", "MKE-P37_900"],
       "SMK-37 packages: listed after the FM-1's, the FM-1 fields unchanged")
    ok(all((out / e["package"]).read_bytes() == p.read_bytes() and e["sha256"] == sha(p) and e["size"] == p.stat().st_size
           for e, p in zip(entries, paths)) and entries[3]["package"] == "firmware/orbit-smk37-mke-0.0.1.fwsc",
       "SMK-37 packages: each published byte for byte as firmware/<name>-<version>.fwsc")
    twice = TMP / "twice.fwsc"
    twice.write_bytes(package(product="SMK-37 Pro_900"))
    rc, _, err = run("--package", str(good), "--package", str(paths[1]), "--package", str(twice), "--version", "0.0.1",
                     "--out", str(TMP / "site-twice"))
    ok(rc == 2 and "same identity" in err and not (TMP / "site-twice").exists(), "two packages with one identity: exit 2, nothing written")
    dual_packages(paths, good, "site-smk-ble")


def dual_packages(paths, ble, label):
    """Generate both releases and compare complete manifests with the input bytes."""
    out = TMP / label
    releases = "https://github.com/dspaudio/orbit/releases/tag/v"
    expected = []
    for inputs, version in ((paths, "0.9.0"), ([ble], "0.9.B")):
        entries = [{"product": I.product_of(path.read_bytes()),
                    "package": f"firmware/{path.stem if i else 'orbit'}-{version}.fwsc",
                    "size": path.stat().st_size, "sha256": sha(path)} for i, path in enumerate(inputs)]
        entry = {"name": "ORBIT", "version": version, **entries[0], "release_url": releases + version}
        if len(entries) > 1:
            entry["packages"] = entries
        expected.append(entry)
    args = [arg for path in paths for arg in ("--package", str(path))]
    rc, _, err = run(*args, "--version", "0.9.0", "--release-url", releases + "0.9.0",
                     "--ble-package", str(ble), "--ble-version", "0.9.B",
                     "--ble-release-url", releases + "0.9.B", "--out", str(out))
    manifest = {**expected[0], "variants": expected}
    ok(rc == 0 and not err and (out / "manifest.json").read_bytes()
       == (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode(),
       f"{label}: complete variants and unchanged default manifest bytes")
    published = [(expected[0]["package"], paths[0]), (expected[1]["package"], ble)]
    published += [(entry["package"], path) for entry, path in zip(expected[0].get("packages", [])[1:], paths[1:])]
    ok(rc == 0 and all((out / name).read_bytes() == path.read_bytes() for name, path in published),
       f"{label}: default, hardware and BLE packages copied byte for byte")
    if rc == 0:
        sums = {line[66:]: line[:64] for line in (out / "SHA256SUMS").read_text().splitlines()}
        ok(sums == {name: digest for name, digest in tree(out).items() if name != "SHA256SUMS"},
           f"{label}: SHA256SUMS covers both releases and every site file")


def ble_failures(good):
    out = TMP / "bad-ble"
    base = ("--package", str(good), "--version", "0.9.0", "--out", str(out))
    options = (("--ble-package", str(good)), ("--ble-version", "0.9.B"),
               ("--ble-release-url", "https://example.test/v0.9.B"))
    for mask in range(1, 7):
        partial = [arg for i, pair in enumerate(options) if mask & (1 << i) for arg in pair]
        rc, _, err = run(*base, *partial)
        ok(rc == 2 and "required together" in err and not out.exists(),
           f"BLE option subset {mask}: exit 2, nothing written")
    cases = [
        (good, "0.9.0", "https://example.test", "BLE version"),
        (good, "v0.9.B", "https://example.test", "BLE version"),
        (good, "0.9.b", "https://example.test", "BLE version"),
        (good, "0.9.B/../bad", "https://example.test", "BLE version"),
        (good, "0.9.B", "http://example.test", "https"),
        (good, "0.9.B", "", "https"),
        (TMP / "missing.fwsc", "0.9.B", "https://example.test", "cannot read"),
        (TMP / "damaged.fwsc", "0.9.B", "https://example.test", "damaged package"),
    ]
    for path, version, url, message in cases:
        rc, _, err = run(*base, "--ble-package", str(path), "--ble-version", version, "--ble-release-url", url)
        ok(rc == 2 and message in err and not out.exists(),
           f"invalid BLE {path.name}/{version}/{url}: exit 2, nothing written")
    occupied = TMP / "site"
    before = tree(occupied)
    rc, _, err = run("--package", str(good), "--version", "0.9.0", "--out", str(occupied),
                     "--ble-package", str(TMP / "damaged.fwsc"), "--ble-version", "0.9.B",
                     "--ble-release-url", "https://example.test")
    ok(rc == 2 and "damaged package" in err and tree(occupied) == before,
       "invalid BLE package leaves an existing generated site unchanged")


def real_package():
    built = ROOT / "build/orbit.fwsc"
    ble = ROOT / "build/orbit-ble.fwsc"
    if not built.exists():
        built = ROOT / "release/orbit.fwsc"
        ble = ROOT / "release/ble/orbit-ble.fwsc"
    if not built.exists():
        print(f"{'build/orbit.fwsc absent: the real-package site was not generated':<72} SKIP")
        return
    out = TMP / "release"
    rc, stdout, stderr = run("--package", str(built), "--version", "0.6.0", "--out", str(out),
                             "--release-url", "https://github.com/dspaudio/orbit/releases/tag/v0.6.0")
    manifest = json.loads((out / "manifest.json").read_text()) if rc == 0 else {}
    ok(rc == 0 and manifest.get("sha256") == sha(built) and manifest.get("size") == built.stat().st_size
       and manifest.get("product") == "FM-1_900" and (out / "firmware/orbit-0.6.0.fwsc").read_bytes() == built.read_bytes(),
       f"build/orbit.fwsc site: {manifest.get('product')} {manifest.get('size')} bytes sha256 {manifest.get('sha256', '')[:16]}...")
    if ble.exists():
        dual_packages([built], ble, "real-dual-release")
        duplicate = TMP / "real-duplicate"
        rc, _, err = run("--package", str(built), "--package", str(ble), "--version", "0.9.0",
                         "--out", str(duplicate), "--ble-package", str(ble), "--ble-version", "0.9.B",
                         "--ble-release-url", "https://example.test")
        ok(rc == 2 and "same identity" in err and not duplicate.exists(),
           "real FM-1 variants cannot bypass the hardware duplicate identity guard")
    else:
        print("real BLE package absent: dual real-package site not generated SKIP")


try:
    good = generated_site()
    failures(good)
    smk_packages(good)
    ble_failures(good)
    real_package()
finally:
    if not failed:
        shutil.rmtree(TMP, ignore_errors=True)
    else:
        print(f"kept {TMP}")
print(f"{failed} failed" if failed else "site tests passed")
sys.exit(1 if failed else 0)
