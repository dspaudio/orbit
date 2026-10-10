# Building ORBIT

The current tree is **ORBIT 0.5.0 / SLOOP v2.5 integration**. Building for the device needs the pi32v2 toolchain and the AC79 SDK. On 2026-10-09 the integrated target compile, `build/orbit.fwsc` generation, memory and existing CPU/ISR budgets were checked, and on 2026-10-10 the displayed version was raised. The [validation record](docs/VALIDATION.md) keeps hardware validation and the post-bump rebuild separate. Generated packages aren't committed to the repository.

The build makes three files in `build/`:

| File | What |
| --- | --- |
| `felucca.bin` | the firmware app |
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

`sh build.sh --release 0.9-beta` makes a release build: the package identity becomes
`FM-1_909` and the version string `0.9-BETA`; the package remains `build/orbit.fwsc`.

Build the 0.5.0 package with `PYTHON=~/.jieli/orbit-venv/bin/python sh build.sh`. The default source version is `ORBIT 0.5.0`, and the install identity is still `FM-1_900`. Check the real displayed version with the device's INFO reply. `--release` only accepts upstream's single-digit `X.Y` form, so don't pass `--release 0.5.0`. For the package, SHA-256 and tag, see the [0.5.0 release](https://github.com/dspaudio/orbit/releases/tag/v0.5.0). The [earlier 0.4.1 release](https://github.com/dspaudio/orbit/releases/tag/v0.4.1) stays available.

Build options (environment, `0` or `1`; defaults in `firmware/src/felucca.c`):

| Flag | Default | |
| --- | --- | --- |
| `FELUCCA_FLASH` | 1 | settings, presets and projects in flash |
| `FELUCCA_OTA` | 1 | update entry (needs `FELUCCA_FLASH`) |
| `FELUCCA_OTA_DRYRUN` | 0 | For checking the OTA flow; never used in a real install package |
| `FELUCCA_CDC` | 1 | USB serial console |
| `FELUCCA_UAC` | 1 | USB audio input: the master output, 44.1 kHz stereo (after Felucca 1.0) |
| `FELUCCA_UART` | 1 | TRS MIDI IN (the 3.5 mm jack) |
| `FELUCCA_ICONS` | 1 if the icon atlas exists | Built-in icons |
| `FELUCCA_SLICE` | 0 | Optional SLICE engine |

## Samples

The CC0 instrument samples that the SAMPLE engine uses are in `assets/samples-cc0/`
(Versilian Studios, see `ATTRIBUTION.txt` there). `tools/fetch_cc0.py` downloads them
again from the source repositories. Without that folder the build still works and the
SAMPLE engine has only the generated drum kit.

## Tests

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

On Windows, use WSL for the command-line build. Installer automation from upstream is not included in ORBIT.

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/orbit.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

This repository doesn't include the web installer site generator `web/make_site.py` or `webapp/installer/`. Use the CLI above to install an `orbit.fwsc` you built yourself. `web/editor.html` edits sounds, the sequencer and samples; it isn't a standalone firmware installer page.

Installing firmware is at your own risk. If an install fails and the FM-1 no longer
starts, recovery needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

