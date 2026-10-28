# Building ORBIT

The current release is **ORBIT 0.8.1**, with selected SLOOP 2.8 / 2.8.3 behavior fixes for the default **FM-1** build. The device owner confirmed normal basic operation after installation. Device builds require the pi32v2 toolchain and AC79 SDK. The [validation record](docs/VALIDATION.md#orbit-081-release-package-2026-10-28) owns package hashes, resource measurements and detailed evidence, with earlier release results kept historical. Generated packages are not committed to the repository.

The build produces these artifacts in `build/`:

| File | What |
| --- | --- |
| `felucca.elf` | the linked firmware app |
| `felucca.dis` | the disassembly used for target-cost checks |
| `loader/ota.bin` | the update loader |
| `orbit.fwsc` | the installable package (app + loader) |

## Prerequisites (macOS)

- Python 3 and a dedicated build environment:

  ```
  python3 -m venv ~/.jieli/orbit-venv
  . ~/.jieli/orbit-venv/bin/activate
  python -m pip install Pillow mido python-rtmidi
  ```
- Docker Desktop. The JieLi toolchain is Linux x86-64 only; the build runs each tool in a
  `linux/amd64` `debian:bookworm-slim` container (Rosetta on Apple silicon). Keep the source
  tree in a folder Docker can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  sh tools/get_toolchain.sh         # installs to ~/.jieli/toolchain
  ```

- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files
  (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); they are not part of this tree.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- Node.js 24 (the version used for the web checks). Node 18 lacks the `CompressionStream("deflate-raw")` the checks use.

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build

```
sh build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations
(`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`).

Build the 0.8.1 package with `PYTHON=~/.jieli/orbit-venv/bin/python sh build.sh`.
The default source version is `ORBIT 0.8.1`, and the install identity remains
`FM-1_900`. **Don't use `--release`:** that upstream option accepts only
single-digit `X.Y` versions, changes the identity and appends BETA. It does
not accept `0.8.1`. Confirm the actual package and source version before
labelling a generated site; the site generator's `--version 0.8.1` supplies
matching metadata, not a firmware version rewrite.

Build options (environment, `0` or `1`; defaults in `firmware/src/felucca.c`):

| Flag | Default | |
| --- | --- | --- |
| `FELUCCA_FLASH` | 1 | settings, presets and projects in flash |
| `FELUCCA_OTA` | 1 | update entry (needs `FELUCCA_FLASH`) |
| `FELUCCA_OTA_DRYRUN` | 0 | For checking the OTA flow; never used in a real install package |
| `FELUCCA_CDC` | 1 | USB serial console |
| `FELUCCA_UAC` | 1 | USB audio input: the master output, 44.1 or 48 kHz stereo |
| `FELUCCA_UART` | 1 (SMK-37: 0) | TRS MIDI IN (the 3.5 mm jack); must stay 0 on the SMK-37 (PH8 is a key row there) |
| `FELUCCA_ICONS` | 1 if the icon atlas exists | Built-in icons |
| `FELUCCA_SLICE` | 0 | Optional SLICE engine |

### Merged SMK-37 target

The default board is still FM-1. SMK-37 builds set `BOARD_SMK37=1` for the app
and update loader and use the SMK-37 flash layout:

| Model | Default build command | Identity | Package |
| --- | --- | --- | --- |
| SMK-37 Pro | `BOARD=smk37 sh build.sh` | `SMK-37 Pro_900` | `build/orbit-smk37.fwsc` |
| SMK-37 Elite | `BOARD=smk37 MODEL=elite sh build.sh` | `SMK-37 Elite_900` | `build/orbit-smk37-elite.fwsc` |
| MKE-P37 | `BOARD=smk37 MODEL=mke sh build.sh` | `MKE-P37_900` | `build/orbit-smk37-mke.fwsc` |

Use these commands without `--release`. The builds share intermediate files
in `build/`; build FM-1 again before `sh tests/run_tests.sh`. SMK-37 has flash
for USR1/USR2 only, and `FELUCCA_UART` must remain 0 because the FM-1 MIDI IN
pin is a key-matrix row there. Use the package for the connected model.
These are source-build options. **The v0.8.1 release does not include
SMK-37 binaries.** Controls and installation safety:
[SMK37.md](SMK37.md).

## Samples

The CC0 instrument samples that the SAMPLE engine uses are in `assets/samples-cc0/`
(Versilian Studios, see `ATTRIBUTION.txt` there). `tools/fetch_cc0.py` downloads them
again from the source repositories. Without that folder the build still works and the
SAMPLE engine has only the generated drum kit.

## Tests

ORBIT 0.8.1 passed the full host suite, native-preview checks and pi32v2
target build and budget checks. All **218 audio renders match the reviewed
goldens**; six hashes changed for the intended SAMPLE and drum fixes.
The separate `orbit_check.py` run has only the known macOS IRQ-test
compilation failure. Detailed results belong in
[docs/VALIDATION.md](docs/VALIDATION.md).

```
sh tests/run_tests.sh
```

Runs the host tests (flash storage, user presets, MIDI parser, update entry, update
loader, a DSP render, the 4-track mix, project formats, the SLICER, the regression suite,
the command-line installer) and, with Node.js, the web page tests. Run it after `sh build.sh`
(it uses `build/` and needs `AC79_SDK` set as for the build).

The regression suite (`tests/regress.c`) renders every engine and preset and compares a
hash of each render with `tests/golden.txt`; it also checks levels, voices and the CPU
cost (`tests/cpu_baseline.txt`, `tests/target_budget.txt`). The host CPU cost is relative
to the idle + drums mix, counted by the kernel on macOS and under callgrind on Linux when
valgrind is installed (exact, about 45 s more); without either it is timed, a rough check.
After an intended change of the sound, `GOLDEN_UPDATE=1 sh tests/run_tests.sh` rewrites
the hashes; `BUDGET_UPDATE=1` does the same for the cost files.

## Install

**BACKUP before updates or downgrades.** Stop playback and export Projects /
Backup from the editor. ORBIT 0.8.0 writes ORP8 projects (3840 bytes) and OPB3
USER preset banks. ORP7 projects and OPB2/UPB1 banks are imported with
parameter-ID migration; legacy ORBIT projects remain readable, but raw SLOOP
FUN6 is rejected. Older firmware, including 0.7.0 and 0.7.1, cannot read the
new formats and may overwrite them through autosave.
Keep a pre-update backup for returning to the matching old firmware. See
[the user guide](GUIDE.md#backup-updates-and-downgrades).

The browser installer address is
[https://dspaudio.github.io/orbit/](https://dspaudio.github.io/orbit/).
The page downloads the latest published stable package, verifies
its size, SHA-256, identity, loader marker and CRCs, then enables Connect.
Connect requests Web MIDI SysEx permission; Install is separately enabled after
device identity preflight. A failed download offers Retry download, not a file
picker. Check that the displayed version is 0.8.1 before installing this
release.

Use Chrome or Edge, a USB data cable and a direct connection. Keep power and USB
connected until the write and reboot finish. On Windows, use WSL for the
command-line build; the upstream INSTALL-SLOOP.bat is not an ORBIT install path.

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/orbit.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

The CLI is the fallback for a checked local `build/orbit.fwsc`. `--info` only
reads identity; the package argument starts the install workflow. The installer
checks the FM-1 identity and package before writing. Neither the CLI nor a
successful simulated installer test proves physical recovery after a power cut.
On 2026-10-28, the exact 0.8.1 release package reached write 100%, rebooted
and returned `FM-1_900` (installer exit 0). The device owner confirmed
normal screen and basic playing operation. Physical OCT- rescue,
measured audio/timing and a firmware-version query remain unverified.

Historical 0.8.0 evidence follows.
On 2026-10-27, the no-demo **0.8 hardware candidate** reached write 100%,
rebooted and returned the `FM-1_900` identity. The device owner confirmed
the normal screen, cleared the earlier autosave with NEW PROJECT and
accepted this hardware check for release. INFO/PING requests timed out;
the successful identity check is not a successful firmware-version query.
The version-only **0.8.0 rebuild has not been separately hardware-tested**.
The 0.7.0 installation/version/PING checks on 2026-10-12, 0.7.1 software
results on 2026-10-15 and 0.6.0 USB audio check on 2026-10-10 remain
historical evidence.

The demo-song loader and DEMO SONG menu are removed in 0.8.0. An existing
working autosave is not erased by the update. Back up any wanted work,
then use **SAVE > TOOLS > NEW** to clear it; the device confirms
**NEW PROJECT**. See [projects and autosave](GUIDE.md#mixer-sections-and-projects).

## Build and serve the installer site locally

After building and checking the actual ORBIT 0.8.1 package
(this local example does not publish a release):

```sh
python3 tools/make_site.py --package build/orbit.fwsc --version 0.8.1 --out build/site \
  --release-url https://github.com/dspaudio/orbit/releases/tag/v0.8.1
python3 -m http.server 8766 --bind 127.0.0.1 --directory build/site
```

For local development with checked SMK-37 packages of the same version,
give `--package` once per package, the FM-1's first
(`--package build/orbit.fwsc --package build/orbit-smk37.fwsc --package build/orbit-smk37-elite.fwsc --package build/orbit-smk37-mke.fwsc`).
They are listed in `manifest.json` under `packages`, and the installer
selects the one for the connected keyboard. These locally built packages
are not part of the official v0.8.1 assets.

Open http://127.0.0.1:8766/ for the installer and
http://127.0.0.1:8766/editor.html for the editor. Localhost is suitable for Web
MIDI; do not open the installer source as a file and expect a release manifest.
Generation copies `web/installer.html` as `index.html`, the installer modules,
editor/font and native screenshot assets. It writes a versioned package,
`manifest.json` and `SHA256SUMS` from that input. Missing or malformed packages
are refused. The output directory must be absent or already a generated site;
regeneration replaces a previously generated site.

Focused site checks:

```sh
node web/test_package.mjs
node web/test_installer.mjs
python3 tests/site_test.py
```

For the first Pages deployment, select **GitHub Actions** as the repository's
Pages source. In the `github-pages` environment, retain the `main` branch rule
and allow release tags matching `v*`: release-triggered runs use the tag ref,
so a main-only environment rejects their deployment.

`.github/workflows/pages.yml` runs after a published release or manual dispatch.
It selects the latest published release, downloads `orbit.fwsc` and `SHA256SUMS`,
checks the checksum, generates/tests the site, then uploads a Pages artifact.
Repository Pages must use **GitHub Actions** as its source. The v0.8.0
release uses the FM-1 `orbit.fwsc` and `SHA256SUMS`; SMK-37 packages are not
release assets. Keep site metadata matched to the actual 0.8.0 package.

Installing firmware is at your own risk. If an install fails and the FM-1 no longer
starts, recovery needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

