#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Generate the static ORBIT installer site from a checked .fwsc package:
  python3 tools/make_site.py --package build/orbit.fwsc --version 0.6.0 --out build/site
The same inputs give the same bytes: the installer as index.html, its modules, the sound
editor with its font, the package as firmware/orbit-<version>.fwsc, the manifest.json the
installer reads (identity, size, SHA-256 of that file) and SHA256SUMS over every file.
--package may be given again for the M-Vave SMK-37 packages of the same release (orbit-smk37.fwsc,
orbit-smk37-elite.fwsc, orbit-smk37-mke.fwsc): each is published as firmware/<name>-<version>.fwsc and
listed in manifest.json "packages"; the installer writes the one for the connected keyboard. With one
package the site is as before. --ble-package, --ble-version and --ble-release-url together add
complete default and BLE release entries to "variants", separate from hardware "packages"."""
import argparse
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_install  # noqa: E402

WEB = ROOT / "web"
INDEX = "installer.html"
COPIED = ["installer.js", "fm1ota.js", "fm1pkg.js", "editor.html", "fukiai.ttf", "FUKIAI-LICENSE.txt",
          "screens/envelope.svg", "screens/preset.svg", "screens/tape.svg"]
VERSION = re.compile(r"\d+\.\d+\.\d+(-[0-9A-Za-z.]+)?")
BLE_VERSION = re.compile(r"\d+\.\d+\.B")


class SiteError(Exception):
    pass


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def checked_package(package):
    try:
        product, _image = fm1_install.load_package(str(package), False)
    except fm1_install.InstallError as error:
        raise SiteError(str(error)) from error
    return product, package.read_bytes()


def release_manifest(package, version, release_url):
    """Check one release's hardware packages and prepare its manifest and file bytes."""
    if release_url and not release_url.startswith("https://"):
        raise SiteError(f"release URL {release_url!r} is not https")
    packages = list(package) if isinstance(package, (list, tuple)) else [package]
    checked = [(path, *checked_package(path)) for path in packages]
    if len({product for _path, product, _raw in checked}) != len(checked):
        raise SiteError("two packages with the same identity")
    if len({path.stem for path in packages}) != len(packages):
        raise SiteError("two packages with the same file name")
    _path, product, raw = checked[0]
    package_path = f"firmware/orbit-{version}.fwsc"
    files = {package_path: raw}
    manifest = {"name": "ORBIT", "version": version, "product": product, "package": package_path,
                "size": len(raw), "sha256": sha256(raw)}
    if len(checked) > 1:                          # the SMK-37 packages: after the first, by their file names
        entries = [{"product": product, "package": package_path, "size": len(raw), "sha256": sha256(raw)}]
        for path, prod, data in checked[1:]:
            rel = f"firmware/{path.stem}-{version}.fwsc"
            files[rel] = data
            entries.append({"product": prod, "package": rel, "size": len(data), "sha256": sha256(data)})
        manifest["packages"] = entries
    if release_url:
        manifest["release_url"] = release_url
    return manifest, files


def generate(package, version, out, release_url=None, *, ble_package=None, ble_version=None, ble_release_url=None):
    if not VERSION.fullmatch(version):
        raise SiteError(f"version {version!r} is not MAJOR.MINOR.PATCH (no leading v)")
    ble_options = (ble_package, ble_version, ble_release_url)
    if any(value is not None for value in ble_options):
        if not all(value is not None for value in ble_options):
            raise SiteError("--ble-package, --ble-version and --ble-release-url are required together")
        if not BLE_VERSION.fullmatch(ble_version):
            raise SiteError(f"BLE version {ble_version!r} is not MAJOR.MINOR.B (no leading v)")
        if not ble_release_url or not ble_release_url.startswith("https://"):
            raise SiteError(f"BLE release URL {ble_release_url!r} is not https")
    manifest, package_files = release_manifest(package, version, release_url)
    if ble_package is not None:
        ble_manifest, ble_files = release_manifest(ble_package, ble_version, ble_release_url)
        manifest["variants"] = [manifest.copy(), ble_manifest]
        package_files.update(ble_files)
    if out.exists() and not (out / "manifest.json").exists():
        raise SiteError(f"{out} exists and is not a generated site: refusing to replace it")
    if out.exists():
        shutil.rmtree(out)
    (out / "firmware").mkdir(parents=True)
    shutil.copyfile(WEB / INDEX, out / "index.html")
    for name in COPIED:
        (out / name).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(WEB / name, out / name)
    for name, data in package_files.items():
        (out / name).write_bytes(data)
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    files = sorted(path for path in out.rglob("*") if path.is_file())
    (out / "SHA256SUMS").write_text("".join(f"{sha256(path.read_bytes())}  {path.relative_to(out).as_posix()}\n"
                                            for path in files))
    return manifest


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--package", type=Path, required=True, action="append",
                        help="the .fwsc to publish; identity, loader and CRCs are checked (again: the SMK-37 packages)")
    parser.add_argument("--version", required=True, help="release version, MAJOR.MINOR.PATCH")
    parser.add_argument("--out", type=Path, default=ROOT / "build/site", help="output directory (default: build/site)")
    parser.add_argument("--release-url", help="https URL of the release page, recorded in manifest.json")
    parser.add_argument("--ble-package", type=Path, help="BLE .fwsc for a separate release variant")
    parser.add_argument("--ble-version", help="BLE release version, MAJOR.MINOR.B")
    parser.add_argument("--ble-release-url", help="https URL of the BLE release page")
    args = parser.parse_args(argv)
    try:
        manifest = generate(args.package, args.version, args.out, args.release_url,
                            ble_package=args.ble_package, ble_version=args.ble_version,
                            ble_release_url=args.ble_release_url)
    except SiteError as error:
        print(f"make_site: {error}", file=sys.stderr)
        return 2
    print(f"{args.out}: {manifest['name']} {manifest['version']} {manifest['product']} "
          f"{manifest['size']} bytes sha256 {manifest['sha256']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
