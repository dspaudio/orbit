# ORBIT validation record

## ORBIT 0.10.0 and 0.10.B release, 2026-10-31

The default package identifies its firmware as `ORBIT 0.10.0`; the Bluetooth
MIDI package uses `ORBIT 0.10.B`. Both keep `FM-1_900`, ORP8, OPB3, `step_t`,
the 104-byte settings object and editor protocol 13. SYNC = OUT uses value 3
of the existing two-bit SYNC field; older firmware reads it as INT.

### Changes and focused tests

| Change | Host evidence |
| --- | --- |
| SYNC = OUT sends MIDI clock, START and STOP (`seq.c` `clock_out_block`) | `midi_clock_test`: START before the first clock, 192 clocks in 4 s at 120 BPM, 96 at 60 BPM right after the change, STOP then silence, nothing with INT, no following of an incoming clock with OUT |
| Controller mode toggles on an OCT- + OCT+ tap; ENV + LFO hold removed (`ctrl.c`) | `ctrl_test`: tap on/off, no toggle on a 1 s hold, on presses 0.3 s apart, or with a key, ENV or LFO held; the tap sends no MIDI |
| Drum GRID: white keys and KNOB 1/2 silent, black keys audition, black keys record while REC plays (`ui_studio.c`, `seq.c`) | `ui_pages_test`: silent white key and knobs, black-key audition, all 11 black-key voices recorded during REC playback, no step change when stopped |
| 0.10.B radio calibration order: stock, kept copy, recovery values (`ble_rf.c`, `ble_vm.c`) | `ble_rf_test` (also under ASan/UBSan): erased VM, damaged kept copy, stock priority, boundary records, persistence, write failure, partial scan |

### Checks

| Check | Result |
| --- | --- |
| Default build, `tools/build.py` with venv Python | Exit 0; app 564,984 B, static RAM 78,760 / 98,304 B, POOL 328,444 / 344,064 B |
| Default `tests/target_budget.py` | Exit 0, budgets unchanged |
| `ORBIT_BLE=1 JIELI_DOCKER=1 tools/build.py` | Exit 0; app 516,860 B, static RAM 92,932 / 98,304 B, POOL 293,904 / 344,064 B; RAM code 7,084 B, 2,362 instructions, all inside RAM code |
| `sh tests/run_tests.sh` with Homebrew LLVM 23 sanitizers and an explicit macOS SDK | Exit 0, `ALL HOST TESTS PASSED`; 218 golden renders, 0 changed, 0 gone, 0 health, voice or CPU failures |
| `tools/orbit_check.py` on macOS | 44 PASS; `irq_init_test` is the known macOS-only compile failure |
| `tests/orbit_preview_test.py`, `node web/test_web.mjs` | Exit 0 |

The BLE package still exceeds the default SAMPLE/GRAIN static-cost
baselines, as in 0.9.B; those baselines are unchanged.

Package SHA-256:

- `orbit.fwsc` (610,032 B): `11ca7174c48c9decdef809af940e1b41d54b7c927abeedafa7b7864ce8dc374b`
- `orbit-ble.fwsc` (610,032 B): `467432ca3f5550256c64bfc8b76c3ec562a96c11dd829c035d9cfbcafb9fca23`

### FM-1 testing

During development, builds carrying these changes (still labelled
`ORBIT 0.9.B`) were installed on the owner's FM-1 with
`tools/fm1_install.py`; each install reached 100%, restarted and returned
`FM-1_900`. The device owner confirmed normal operation, Bluetooth, the
OCT tap, the quiet GRID and clock out. The released 0.10.B differs from the
last of those builds only in its version string. The recovery calibration
path has not run on a unit whose stock calibration is erased; the owner's
unit has its stock record. Clock jitter, drift, Bluetooth latency and audio
under radio load were not measured. Logs are in `build/verification/0.10/`.

## ORBIT 0.9.0 and 0.9.B candidates, 2026-10-30

These are local candidates, not published or hardware-qualified releases.
The default package identifies its firmware as `ORBIT 0.9.0`; the separate
Bluetooth MIDI package uses `ORBIT 0.9.B`. Both retain `FM-1_900`, ORP8,
OPB3, `step_t` and editor protocol 13. The controller settings extend PER3
from 88 to 104 bytes; legacy settings readers and the web backup validator
have focused compatibility tests.

### Checks completed so far

| Check | Result |
| --- | --- |
| Default target build, venv Python and `sh build.sh` | Exit 0; app 564,888 B, static RAM 78,760 / 98,304 B, POOL 328,444 / 344,064 B |
| Default `tests/target_budget.py` | Exit 0 with unchanged budgets and +10% tolerance; SAMPLE 125 / 126, GRAIN 1764 / 1734, NOISE 286 / 264 |
| `sh tools/check_default_isolation.sh` | Exit 0; two fresh snapshots produce byte-identical default `felucca.bin`, including when every BLE-only file is deleted |
| Golden regeneration and subsequent `regress` | User-approved update of all 218 hashes; subsequent run: 0 changed, 0 gone, 0 health/voice/routing/CPU failures and 0 crashes |
| `node web/test_web.mjs` | Exit 0, including 104-byte PER3 backup acceptance and invalid-size rejection; reverting the validator makes the new assertion fail |
| `python3 tools/orbit_check.py` before the final loop-cost optimization | 44 PASS; the unchanged Linux-only `irq_init_test` cannot compile on macOS |
| `irq_init_test` with Linux amd64 `gcc:13` | Exit 0; cold/warm divide-trap masking preserves unrelated bits, ETM and vectors |
| Integrated `sh tests/run_tests.sh` with Homebrew LLVM sanitizers and explicit macOS SDK | Exit 0; includes the new SLOOP fix tests, editor fuzz, BLE codec/bridge/stand-in checks, golden regression, default target budgets and web checks |
| Final `ORBIT_BLE=1 JIELI_DOCKER=1 python3 tools/build.py` with explicit `AC79_SDK` | Exit 0; `build/orbit-ble.fwsc` 610,032 B, app 516,500 B, static RAM 92,932 / 98,304 B, POOL 293,904 / 344,064 B |
| Final BLE RAM-code gate | 7,084 B; 2,362 instructions and 211 calls, all confined to RAM code with no XIP reference |
| `python3 tests/orbit_preview_test.py` | Exit 0 |
| Full `python3 tools/orbit_check.py` in a disposable Linux amd64 `gcc:13` container | Exit 0; all 45 cases PASS, including `irq_init_test` |

The BLE package passes storage limits, not the default SAMPLE/GRAIN cost
baselines: the latest optimization report records BLE costs of 304 and
2074 respectively, versus default budgets of 126 and 1734. Those baselines
remain unchanged. Host CPU measurements are not radio-load timing proof.

### Authorized 0.9.B installation

On 2026-10-30, the connected FM-1 answered the installer's identity request
as `FM-1_900` on port `Felucca`. Pre-install INFO and PING were unreliable,
but BK_LIST/BK_GET succeeded with bounded same-connection retries. All 15
backup objects passed CRC checks, including the 3,840-byte ORP8 working
project, 88-byte settings, 1,464-byte SYN kits and 40-byte MIDI settings.
The backup is retained at `build/backups/pre-0.9.B.json`; its firmware
metadata explicitly records that INFO was unavailable.

The user authorized installation after tests. The command
`python3 tools/fm1_install.py --port Felucca --yes build/orbit-ble.fwsc`
completed with exit 0: writing 100%, restart, then
`done: the FM-1 runs FM-1_900`.
Installed package SHA-256:
`4aad8a7c8d244568cf7ea606f9297c01aedda05c8ffeadf4001fcdd6e854c0ae`.
Default package SHA-256:
`e2c28bcdbe56ddfd1cfa626e6bdfc3062b051c2f5cffa7a5413599f6454f03eb`.
The model identity alone is not a version or BLE functionality check.
After installation, the read-only INFO query returned
`FELUCCA ORBIT 0.9.B` (17 engines, protocol 13), confirming the running
firmware version. The first request timed out; the second request on the
same MIDI connection succeeded.

Logs and lane reports are retained in `build/verification/0.9/`.
The temporary run/lane directories and the downloaded `gcc:13` image were
removed after preserving evidence; no verification container remains.
The retained controller and Bluetooth error-screen host renders show no
overlapping labels. These images do not prove a physical BLE connection.
Static RAM headroom is not total runtime free RAM; POOL, main/IRQ stacks
and BLE task stacks are separate. No physical stack high-water, radio
pairing, MIDI delivery, latency or audio-under-radio-load measurement has
been made.

### Intended audio changes and golden attribution

The SLOOP 2.8.4 ports originate at `isod89/sloop-fm1` commit `91f3018`.
Individual lane reports record isolated comparisons against ORBIT `5824b5b`.
Rounded part/master mixing changes every one of the 218 existing golden
rows. The following rows additionally change under their respective fix
alone:

| Fix | Additional affected rows |
| --- | --- |
| UNISON oscillator-state spread | `preset/ANALOG/12_SUPERSAW`, `mode/DIGITAL/UNISON` |
| PHYS string-loop phase compensation | `preset/PHYS/02_STR_PLUCK`, `07_SITAR`, `08_PHYS_HARP`, `09_NYLON_GTR`, `10_STEEL_GTR`, `11_MUTED_GTR`, `12_PLUCK_BASS`, `13_KOTO`, `14_BANJO`, `16_TANPURA` (all under `preset/PHYS/`) |
| NOISE high-rate LFSR and colour bounds | `preset/NOISE/02_NZ_ARCADE`, `preset/NOISE/10_RADIO` |
| GRAIN held-note scheduling | `preset/GRAIN/00_LOFI_CLOUD`, `preset/GRAIN/04_SHIMMER` |

SAMPLE/GRAIN pitch-limit fixes alone, input sanitation, controller mode
and BLE integration produce no additional default golden changes in the
isolated lane comparisons. The final loop-cost optimization preserves all
218 pre-optimization hashes. CPU baseline and target-budget files were not
updated.

The first integrated target check caught SAMPLE 593 / 126, GRAIN
2387 / 1734 and NOISE 308 / 264 overruns despite passing host tests.
The correction separates SAMPLE block preparation from its checked sample
loop, prevents per-block grain spawning from being inlined into the render
loop, replaces its wide span multiplication with equivalent bounded
32-bit arithmetic, and separates NOISE's integer clock count from its
fractional phase before the sample loop. The static checker does not
establish total execution time or account fully for out-of-line callees.

## ORBIT 0.8.2 release package, 2026-10-29

This release adds the delay **TYPE** (MONO / WIDE / PING), removes the
full-screen visualizer and ports SLOOP 2.7 memory changes (isod89/sloop-fm1
`ba54ba5`, from the sloop-lab RAM branch by Andre Rezemini). Baseline HEAD:
`514eb60` (0.8.1). The default build used
`PYTHON=~/.jieli/orbit-venv/bin/python sh build.sh`, not `--release`, and
embeds `FELUCCA ORBIT 0.8.2`. Identity/board remain `FM-1_900` / `fm1`.
ORP8, OPB3, the `step_t` layout and editor protocol 13 are unchanged; the
protocol reports 46 globals.

### Package and measured resources

| Item | 0.8.1 | 0.8.2 |
| --- | --- | --- |
| `build/orbit.fwsc` | 610,032 B | 610,032 B |
| App image (`felucca.bin`) | 566,360 B | 557,192 B |
| Static RAM `.data + .bss` | 97,544 / 98,304 B (760 B free) | 78,584 / 98,304 B (19,720 B free) |
| Pool | 333,948 / 344,064 B | 328,444 / 344,064 B |

Package SHA-256:

```text
45f43de713e9c03ee9b5d130eb7793cbd7f9e2b85206288e4f5cfde10e5be3da
```

`felucca.bin` SHA-256:
`f3c729bafbe238d71d762521155e1388f6565e95e794a59ea193698ff39c5783`.
As before, static RAM headroom is not total runtime free RAM: POOL and the
stacks are separate, and stack high-water usage is unmeasured.

| Change | Static RAM effect |
| --- | --- |
| Visualizer (`ui_vis.c`) removed; right scope buffer and note taps gone | `.bss`, and 5,504 B of pool |
| `midi_sel_on` packed as nibbles (SLOOP 2.7) | 1,024 B |
| `scratch.h`: storage payload and HOME scope snapshot share one buffer | 1,024 B |
| USER preset banks read in the flash XIP window; editor restore converted in place | 9,528 B (`up_bank` + `ed_bk_commit.nb`) |
| DRAWBAR state moved into the existing per-part engine arena | about 2.2 KB, pool unchanged |
| Delay TYPE state | +16 B; the delay line is shared, pool unchanged |

At boot, a USER bank still stored as OPB2 or UPB1, or an OPB3 bank with a
record that would be cleared, is converted and written back once as OPB3, so
the flash copy holds what the RAM mirror held. A second start writes nothing.
The delay TYPE is stored in bits 14..15 of the project's `g[G_DRCH]` slot,
beside TEXTURE's LEVEL and TONE; 0.8.1 masks those bits and earlier projects
read as MONO. The settings word writes 0 in the former visualizer style bits
17..20; older firmware reads that as OSCILLOSCOPE. SLOOP's `UI_SMALL` (clang
`minsize` on UI functions) was tried and dropped: the pi32v2 build was
byte-identical with and without it.

### Checks

| Check | Result |
| --- | --- |
| `python3 tools/orbit_check.py` (venv Python with Pillow) | 35 PASS / 1 FAIL; the failure is the known macOS `irq_init_test` compile limitation |
| `python3 tests/orbit_preview_test.py` | exit 0 |
| `sh tests/run_tests.sh` | every host, regression and web test passed, including `delay_type_test`; **218 golden renders, 0 changed**. It exited 1 only because Apple clang's AddressSanitizer aborted at start-up in the two ASan steps |
| ASan + UBSan with Homebrew LLVM and the macOS SDK | `stress_asan 15000 7`, `arena_asan` (`ARENA_STATS`, ownership violations 0) and `delay_type_test` exit 0 |
| `python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt` | exit 0. Against 0.8.1: `drawbar_render` 267 -> 271 (within +10 %), `drawbar_block` 102 -> 103, `fm1_alnk0_irq` 138 -> 126; other costs unchanged |

New host checks: `delay_type_test` (MONO centred; WIDE right echo 573
samples later; PING left, right, left with falling level; PING time clamped to
half the line; no stale echo after a TYPE change), `project_test` (TYPE round
trip; a 0.8.1 project reads MONO with TEXTURE intact), `macro_test` (USER bank
read in place from a simulated NOR; OPB2 converted once and not again) and
`upreset_test` (OPB2, UPB1 and OPB3 converted in place as through a second
buffer).

### Installation on the FM-1, 2026-10-29

`python3 tools/fm1_install.py build/release-0.8.2/orbit.fwsc --yes` (exit 0):
18 package reads, update mode, write 100%, restart, `done: the FM-1 runs
FM-1_900`; `--info` then reported `FM-1_900 [running]`. The device owner
confirmed that the FM-1 started and worked normally and that the delay's
MONO, WIDE and PING types sounded as expected, and approved release. No
firmware-version query was made. Measured audio quality, ISR timing and
underruns, flash cache behaviour after a save measured on the device, and
physical OCT- power-on rescue were not tested.

## ORBIT 0.8.1 release package, 2026-10-28

This release ports selected SLOOP 2.8 / 2.8.3 behavior, with credit to
the SLOOP authors. The default FM-1 software integration passed, with the
documented macOS IRQ-check exception below. The package was subsequently
installed on an FM-1. The device owner confirmed normal screen and basic
playing operation and authorized release.
See the [release notes](releases/0.8.1.md).

Local integration evidence is preserved in `build/host/orbit-081-evidence/`:
`integrate.md` and the `drums.md`, `voice.md`,
`seqb.md`, `layer.md` and `rescue.md` lane reports. Baseline HEAD:
`3cbd2502ddd375484574837ae8e7e6a46c5c1e2d`. The default build used
`sh build.sh`, not `--release`, and embeds `FELUCCA ORBIT 0.8.1`.
Identity/board remain `FM-1_900` / `fm1`. ORP8, OPB3, the `step_t` layout
and flag meanings, and editor protocol 13 are unchanged.

### Package and measured resources

| Item | Measured result |
| --- | --- |
| `build/orbit.fwsc` | 610032 bytes |
| `build/felucca.bin` | 566360 bytes |
| `build/loader/ota.bin` | 6876 bytes |
| Static RAM `.data + .bss` | 97544 / 98304 bytes; 760 B region headroom |
| Pool | 333948 / 344064 bytes; 10116 B region headroom |
| `.ram_text` | 925 instructions, no calls |

Package SHA-256:

```text
77fd12eefc0c2e156a5bea73c0856b2279166068452d2b30a76d358c11b1149c
```

The **760 B** is unused capacity in the static `.data`/`.bss` RAM region,
not total runtime free RAM. The [linker map](../firmware/app.ld) places
POOL in a separate region and reserves user/supervisor stacks separately.
Actual stack high-water usage is unmeasured. Neither these static figures
nor the target instruction-cost checks prove hardware timing.

Existing target-budget checks passed without changing their baselines.
`drums_mix` measured cost 6563 against budget 6153 and passed within the
checker's tolerance. The final target register-access check also passed.

### Software checks

The integration runner recorded these actual exit codes:

| Command | Exit | Result |
| --- | ---: | --- |
| `PYTHON=$HOME/.jieli/orbit-venv/bin/python sh build.sh` | 0 | Default pi32v2 FM-1 package built |
| `python tests/target_budget.py build/felucca.dis tests/target_budget.txt` | 0 | Existing target budgets passed |
| `strings build/felucca.bin \| grep "FELUCCA ORBIT 0.8.1"` | 0 | Embedded version matched |
| `python tools/orbit_check.py` | 1 | 34 PASS / 1 FAIL; only `irq_init_test` failed |
| `python tests/orbit_preview_test.py` | 0 | Native DSP/HTTP preview: 44100 Hz stereo, non-silent sequence, panel events and invalid-input checks passed |
| `sh tests/run_tests.sh` | 0 | `ALL HOST TESTS PASSED` |
| `node web/test_web.mjs` | 0 | `web tests passed` |

The Python commands used `$HOME/.jieli/orbit-venv/bin/python`. The full
suite used that environment on PATH and
`CC_UB="/opt/homebrew/opt/llvm/bin/clang -isysroot $(xcrun --show-sdk-path)"`.
It retained the default 10-minute simulated soak, 40000-frame stress,
15000-frame ASan/UBSan stress, arena sanitizer and divide-by-zero checks;
none was shortened. The arena check covered 128 engine pairs with
bit-identical fresh-start results.

The separate `orbit_check.py` run is **not all green**: `irq_init_test`
cannot compile on macOS because its section attribute is not valid
Mach-O syntax and `MAP_FIXED_NOREPLACE` is unavailable. The test was not
deleted, skipped or weakened. Existing preview macro-redefinition and
32-bit MMIO pointer-cast warnings remain.

Focused checks passed for drum allocation (kits 0, 5 and 42), SAMPLE
retrigger/voice budget, live-held POLY and MIDI overflow (11 cases),
external clock (4 cases), layer timing, and recovery. Layer tests confirm
no page during or after 0.2 s, display at 0.4 s, and immediate display on
held-knob use for the 350 ms threshold. The final rescue path uses the HAL
backlight helper before the early key scan; the lane report's direct
register-write description predates that correction.

### Reviewed audio changes

Six of 218 golden hashes changed relative to the baseline: five SAMPLE
renders due to fade-out and restart from silence, and one drum render due
to voice selection when all drum voices are busy. The other 212 are
unchanged; none was added or removed.

| Render | Baseline hash | Release hash |
| --- | --- | --- |
| `preset/SAMPLE/00_GRAND_PNO` | `d923a2cd58dd6c07` | `9c5d53a1ab12d0fc` |
| `preset/SAMPLE/02_VIBES` | `b990857c2a729642` | `6e14271dc9c6ebf0` |
| `preset/SAMPLE/05_LOFI_FLUTE` | `9e8271d7e51eedda` | `4f9e9c62f7c4dff6` |
| `preset/SAMPLE/08_DUSTY_PNO` | `3918c5839bb7a848` | `09c7cc02bc9e2468` |
| `preset/SAMPLE/09_LOFI_KEYS` | `6fa2f6b1e30b1f3d` | `9c85a710ae9edfa6` |
| `drums/gm_kit` | `97750328f0896d25` | `b56f39d6452ad27c` |

Source-isolation runs attributed the changes: baseline `voice.c` with
the drum fix left only the drum mismatch; baseline `drums.c` with the
voice fix left only the five SAMPLE mismatches; both baseline files
matched all 218 original renders. The final suite matched all reviewed
goldens, with zero health failures, voice/routing failures, CPU entries
over budget or crashes.

### FM-1 installation and remaining checks

On **2026-10-28**, the package with the SHA-256 above was installed using:

```sh
python tools/fm1_install.py build/orbit.fwsc --port Felucca --yes
```

The installation monitor returned **exit 0**. The inspected log,
`build/host/orbit-081-install.log`, records package checking, write 100%,
`restarting`, and `done: the FM-1 runs FM-1_900`.

This confirms the update, restart and returned identity. The device owner
subsequently confirmed normal screen and basic playing operation and
approved release. A displayed ORBIT version or firmware-version query,
measured audio quality/timing and OCT- power-on rescue remain unverified.

The default build has **`FELUCCA_SLICE=0`**; neither a SLICE-enabled build
nor SLICE retrigger behavior was tested. SAMPLE is the tested engine.
Sequencer changes protect **live-held POLY** notes only. The original
storage-dependent continuation request in `seq.md` remains superseded
and excluded; `seqb.md` covers the implemented runtime ownership, MIDI
overflow and clock behavior, not a stored continuation feature.

## ORBIT 0.8.0 release package, 2026-10-27

The final default build embeds `FELUCCA ORBIT 0.8.0` and retains identity
`FM-1_900`. The version-only rebuild after user acceptance of the 0.8
no-demo candidate has an app of 564888 bytes, RAM 97272/98304 bytes and
pool 333948/344064 bytes. Existing target-cost budgets passed unchanged.
The package is 610019 bytes, SHA-256:
`6103c21d9b0f5268568ae3b8be0bd59c38021545446317640f59e2fbed38dbaf`.

The underlying implementation passed the complete host regression suite with
218 unchanged audio goldens, plus filter, migration, macro and sanitizer
checks described below. The installed no-demo candidate was accepted by the
user; its restored working patterns were cleared using NEW PROJECT, without
disabling autosave restoration. The final version-only 0.8.0 binary has not
been separately installed. Editor INFO/PING and physical audio/timing claims
are not inferred from the updater's identity response.

## ORBIT 0.8 hardware candidate, 2026-10-27

This unpublished candidate adds track FILTER RES and BP, ORP8 project migration
and OPB3 preset banks, and removes the built-in FIRST LIGHT loader and DEMO SONG
menu. Source version: `ORBIT 0.8`; prospective release: `0.8.0`, pending the
user's hardware check.

The no-demo pi32v2 build passed with an app of 564888 bytes, RAM 97272/98304
bytes and pool 333948/344064 bytes. All existing target-cost budgets passed.
The `FM-1_900` package is 610019 bytes, SHA-256:
`f8c6c766824b9c2090e9c5b392dcb5b41c9d4822ff298766a361a78ad2ed62db`.

The full host suite passed using Homebrew LLVM with the explicit macOS SDK.
All 218 audio goldens remained unchanged, with zero health, routing, CPU-budget
or crash failures. Native preview and web tests passed. Focused filter tests
cover resonance, BP response, bypass ramps and default-output compatibility;
storage tests cover old-format migration and RES/TYPE macro persistence.
The separate `orbit_check.py` IRQ test retains its known macOS incompatibility.
After removing the demo menu, its stale UI fixture was corrected and the full
suite passed again (`build/host/issue5-nodemo-full.log`).

The CLI updater installed this exact no-demo package over USB MIDI: package
check, write 100%, restart and returned `FM-1_900` identity succeeded (exit 0).
The user reported no data requiring preservation. An earlier candidate showed
a normal ORBIT screen according to the user. Editor INFO requests timed out;
the displayed version and listening/timing behavior of the final no-demo
candidate remain for the user's physical check. No GitHub publication was made.

## ORBIT 0.7.1 release package, 2026-10-15

The default pi32v2 build reports `FELUCCA ORBIT 0.7.1` and retains install
identity `FM-1_900`, editor protocol 12, 17 engines and the existing factory
bank/storage formats. This is a software-validated package, not a new
physical-device installation.

| Region | Used | Limit | Remaining |
| --- | ---: | ---: | ---: |
| App image | 563992 B | 581564 B | 17572 B |
| RAM `.data + .bss` | 97064 B | 98304 B | 1240 B |
| Pool | 333948 B | 344064 B | 10116 B |

The app is 15768 bytes smaller than the published 0.7.0 image. Binary font
packing and glyph sharing retain the source glyph pixels; unused code/data
sections and shared screen-transfer setup remove a further 636 bytes from
the preceding UI/font candidate. No sample bank, factory preset or sound
engine was removed. RAM and pool use are unchanged, and the pool retains
the required 8192-byte reserve.

`orbit.fwsc` is 610019 bytes, SHA-256
`842a69569eaedd0e063e8caa13e7de5f6fb0412bed5b6fccbf11ed3dbdd8c68d`.
The app SHA-256 is
`34612d49498fc1e14f58fde7a499e9769d789010daea807de2c2c73099e75011`.
The package pads the fixed app slot, so its download size is unchanged.
The source default version was changed before building; no `--release`
identity override or metadata-only version rewrite was used.

`sh tests/run_tests.sh` passes in full with the installed Homebrew LLVM
23.1.3 sanitizer compiler and an explicit macOS SDK:

```sh
PATH="$HOME/.jieli/orbit-venv/bin:$PATH" \
CC_UB="/opt/homebrew/opt/llvm/bin/clang -isysroot $(xcrun --show-sdk-path)" \
sh tests/run_tests.sh
```

This covers 218 unchanged audio golden renders, zero health/voice/routing
failures, zero host CPU budget overruns, all 35 unchanged pi32v2 static-cost
budgets, storage/update/installer/web checks, divide-by-zero checks, the
10-minute simulated-use soak, and ASan/UBSan stress/arena checks. The default
Apple ASan startup issue documented below is avoided by using the working
LLVM runtime; no test or budget is skipped or weakened.

The required `python tools/orbit_check.py` run passes 29/30 cases on macOS.
Its unchanged Linux-only IRQ initializer cannot compile on Mach-O or use
`MAP_FIXED_NOREPLACE` there; the original test passes separately with GCC
on Linux x86-64. The required runner's macOS exit code is therefore 1,
not an all-green native run.

`python tests/orbit_preview_test.py`, `python tests/site_test.py`,
`node web/test_package.mjs` and `node web/test_installer.mjs` all pass.
The real native preview produces non-silent 44100 Hz stereo audio and
validates panel routing, scope output and invalid-input handling.
The generated 0.7.1 installer manifest names `FM-1_900` and exactly matches
the release package's size and hash above.

Host UI regression covers the drum matrix, voice/step key routing, short
pages, recording from GRID, step automation capacity/Undo, Tape shortcuts,
pattern rotation/doubling metadata, SYSTEM layout and long SONG entries.
Actual C preview captures at desktop 1440px and iPhone 390px confirm the
0.7.1 label and drum matrix; the phone document has no horizontal overflow.
These captures and audio checks do not establish FM-1 timing or sound quality.

The previous 0.7.0 device installation and 0.6.0 USB recording remain
historical evidence only. No 0.7.1 installation, hardware audio/timing,
power-loss recovery, physical controller test or crackle resolution is
claimed.

## ORBIT 0.7.0 package and device installation, 2026-10-12

The 0.7.0 package was built locally and installed on an FM-1 at the user's
request. Installation and device responses are verified below; publication
and Pages deployment are separate release steps.

The actual pi32v2 build passes with identity `FM-1_900`:

| Region | Used | Limit | Remaining |
| --- | ---: | ---: | ---: |
| App image | 579760 B | 581564 B | 1804 B |
| RAM `.data + .bss` | 97064 B | 98304 B | 1240 B |
| Pool | 333948 B | 344064 B | 10116 B |

The pool retains the required 8192-byte reserve. All 35 static target-cost checks
pass with existing budgets and +10% tolerance unchanged; only the new CLOUD
baselines were added (render 451, block 9). These are instruction-cost estimates,
not hardware cycle or ISR-deadline measurements.

`build/orbit.fwsc` is 610019 bytes, SHA-256
`af8ecb95e2a15e11a2ac102664c1d47df2454ebcd5b163110f27cd775a591ddf`.
The actual ELF places four 3840-byte project slots at `0x01c7c0c8`,
with `_bss_end=0x01c1fb28` and `_pool_end=0x01c7187c`.

Host evidence:

- `python tools/orbit_check.py`: 29/30 cases pass on macOS. The unchanged
  Linux-only IRQ initializer test passes separately on Linux x86-64; macOS
  cannot compile its ELF section attribute or `MAP_FIXED_NOREPLACE`.
- The full `sh tests/run_tests.sh` run exposes an existing Apple ASan startup
  error at `sanitizer_malloc_mac.inc:189`. An empty `main` reproduces it.
  The unchanged ASan/UBSan stress and arena tests pass on Linux arm64.
- Strict audio regression passes: 218 matching hashes, zero health failures,
  voice/routing failures, CPU overruns or crashes. The deliberate look-ahead
  limiter changes the previous 210 hashes; exactly eight CLOUD rows were added.
  Existing host CPU budgets and tolerances remain; only eight measured CLOUD
  entries were added.
- Project, user-preset, MIDI-learn, UI, INSERT, modulation, punch, slicer,
  sequencer and CLOUD boundary checks pass with divide-by-zero sanitizers.
  CLUSTER arithmetic also passes full undefined-behavior checks against
  independent wide oracles: 1720896 products and 1720896 phase/control pairs.
- Web regression covers protocol 12, legacy 0.6.0, signed16 CLUSTER values,
  macro persistence, source namespaces and rejection before any MIDI request
  when a custom mapping cannot be sent to pre-v12 firmware.
- Final audit found that the numeric ENV/LFO multiplier also accelerated
  named ENV SYNC divisions. A real-panel regression reproduced nine failures;
  exempting named descriptors fixes slow and fast detents in both directions
  for ATK/DEC/REL while retaining accelerated unsynced controls. The required
  host checker again passes 29/30, with only the same macOS IRQ compile limit.

Fresh native C captures confirm MACROS assignments, PERFORM values and the
unchanged HOME Event Tape / Visualizer cycle. The 624-condition CLUSTER
diagnostic reports zero POLY voice-retention failures and zero full-mix endpoint
PCM clipping in its 576 full-mix conditions. MONO/LEGATO cases intentionally
have one voice. In sixteen four-note default-FX conditions, samples above the
output soft knee drop from 16808 to 7033 with the new limiter.

The bounded multiply / signed-shift optimization retains the original PCM and
state behavior. Alternative phase-width and forced-inline candidates passed
host math/audio checks but exceeded the unchanged pi32v2 cost budget; neither
is shipped. The accepted arithmetic was retained in the final rebuild with
the ENV SYNC input correction; image, RAM, pool and static costs are unchanged.

The reported physical CLUSTER one-voice/crackle symptom was not reproduced.
Every factory CLUSTER preset is already POLY; MONO/LEGATO and CPU shedding are
distinct possible one-voice paths. Host checks do not prove hardware timing or
crackle resolution. MiniLab 3 relative encoders and physical controller
operation were not tested; the learned map is device-wide and absolute,
not an unlimited or per-project controller profile.

### Actual 0.7.0 device installation

The verified package above was installed over USB MIDI on `FM-1_900`
(Felucca), previously running ORBIT 0.6.0. Transfer reached 100%, automatic
reboot completed and the installer exited 0. A separate device probe required
the exact version `FELUCCA ORBIT 0.7.0` and returned protocol 12, 17 engines,
45 globals and a successful PING.

All 14 pre-install and 15 post-install backup objects passed independent
decoded-length and CRC-32 checks. Thirteen previous objects are byte-identical;
none were removed. The new 40-byte MIDI settings object is ID 10. Working
project object 0 remains 3840 bytes and converts from FUN5 to ORP7. Decoded
comparison preserves four tracks' legacy sound values, engine/preset IDs,
steps, microtiming, fill and lock semantics, selection and applicable globals.
The repurposed G_INFO/G_DRCH storage cells encode default OFF TEXTURE metadata;
raw project byte equality is not claimed.

Personal backups remain private and are not release assets. This installation
does not establish audio quality, CLUSTER crackle resolution, target deadlines,
power-loss recovery or physical controller behavior.

## ORBIT 0.6.0 release preparation, 2026-10-10

README, release notes, public technical documentation and new comments are in English.
After translating generator comments and diagnostics, the pi32v2 rebuild and all
33 static target cost checks passed again. The rebuilt `orbit.fwsc` is byte-identical
to the installed package below, retaining SHA256
`460042548a8b915ee11063c9f0df67d9c6c1a79a73c48b6cefac1eb91180b11e`.
The public source uses only its checked-in cluster assets for generation, not private
extraction records. The generated header is identical across two runs, SHA256
`299bb466d856e220b978ac848d83ff49212fa2ebd45bc2f44d0854299278b429`.
The earlier header hash below records the generation before this prose-only translation.
Private backups, recordings and work journals are excluded from commits and release assets.
The release assets are `orbit.fwsc` and `SHA256SUMS`; hardware verification scope remains
the installation and short USB test described below.

## ORBIT 0.6.0 hardware install and USB audio check, 2026-10-10

At the user's request, the source, native preview and web mock versions were bumped to 0.6.0 and installed on the user's `FM-1_900`, which was running ORBIT 0.5.0.
Package transfer reached 100%, and the automatic reboot and installer exit 0 were confirmed.
The current source includes the official CLUSTER oscillator, its 16 raw presets, editor v11 and the synthesized FLUTE/SCRCH replacement sounds.

| Check | Result of a direct run |
|---|---|
| Real reboot | INFO `FELUCCA ORBIT 0.6.0`, protocol **11**, engines **16**, globals **33**, PING PASS |
| Data preservation | Backups taken before install, after install and after the sound check: **14/14 objects bit-identical**, every object's CRC PASS. Existing patterns, settings and user bank data preserved |
| Real CLUSTER | NAMES matches all **16** original names. Applying each PRESET reads back **8 signed16 raw values** that all match the original factory JSON |
| Real MIDI | A 3-note phrase on channel 1 sent to **19 sounds** (the 16 CLUSTER presets plus LOFI FLUTE / SCRATCH / FLUTE DUST), PING PASS after each phrase. Afterwards, track 1's original engine/preset/61 parameters and the selected track were restored and compared, PASS |
| Real USB output | QuickTime input explicitly set to **Felucca**, recording **38.164 s at 48,000 Hz stereo**. After PCM conversion: **1,831,872 frames**, left/right peak **2,574 / 2,574**, RMS **273.92 / 275.31**, full-scale clipped samples **0**. This is the device's USB output, not a host DSP render |
| pi32v2 build | image **548,960/581,564 B**, RAM **85,908/98,304 B**, pool **333,948/344,064 B**, package **610,019 B**, identity `FM-1_900`, exit 0 |
| Static target cost | **33 PASS**, existing baselines and tolerances kept, exit 0 |
| Strict macOS regression | **210 renders**, changed/gone, health, voice/routing, CPU overrun and crash all **0**, exit 0 |
| Required host checks | Standalone Linux amd64 `tools/orbit_check.py` **24/24 PASS**. macOS gives **23/24 PASS** because of the existing IRQ Mach-O/MMIO compile limit; that failure wasn't hidden and the test wasn't changed |
| Preview, web and installer | `tests/orbit_preview_test.py`, `node web/test_web.mjs` and `tests/install_test.py` all exit 0. Native stereo/HTTP/panel checks PASS |

Installed `build/orbit.fwsc` SHA256:
`460042548a8b915ee11063c9f0df67d9c6c1a79a73c48b6cefac1eb91180b11e`.

Private backups, check JSON and the hardware recording are kept locally in
`.omo/evidence/orbit-0.6.0-install-20261010/`.
`fm1-060-usb-audio.m4a` and the converted `.wav` are the real USB recording of the MIDI check above.
Because the input device was selected explicitly, the host microphone wasn't recorded. The recording and playback windows were closed afterwards.

This check confirms a short 48 kHz USB output, control responses and data preservation.
It doesn't cover per-sound quality judged from separate files, listening by the user, headphone/line output,
real ISR execution time, audio dropouts under worst-case load, long-term stability, power-cycling or recovery. No commit, tag, public release, or web emulator/Dots deployment was made.
Earlier full Linux and sanitizer results stay in the work-stage entries below,
kept apart from the checks rerun after this version change.

## FLUTE/SCRCH synthesized replacement sounds, 2026-10-10

At the user's request, the silent entries were restored with replacement sounds.
FLUTE is a periodic synthesized loop of a fundamental plus weak harmonics.
SCRCH is a one-shot of fixed-seed noise with a frequency sweep.
Both are synthesized at build time and stored as IMA ADPCM; no new runtime synth engine was written.
`LOFI FLUTE`, `SCRATCH` and `FLUTE DUST` play through their original SAMPLE/GRAIN paths.
The original bank/zone/preset/USR numbers, roots/key zones and the FUN5 layout are kept.

| Check | Result of a direct run |
|---|---|
| Data and compatibility | FLUTE 6,172 B + SCRCH 3,072 B = **9,244 B**, a net saving of **45,040 B** against the original 54,284 B. ADPCM hashes of the kept banks, 8 sets/51 zones, preset/USR numbers and roots/key zones preserved |
| Real SAMPLE/GRAIN | Both banks are non-silent on all 128 MIDI notes, output bounds guard PASS. FLUTE's valid loop and SCRCH's one-shot confirmed |
| Strict macOS DSP | **210 renders**, changed/gone, health, voice/routing, CPU overrun and crash all **0**, exit 0. CPU baselines and tolerances unchanged, 3 informational faster-item notes |
| Three presets | peak **18,457 / 21,339 / 19,095**, DC **1.3 / 0.0 / 1.9**, tail peak/DC after release **0**. Order: LOFI FLUTE / SCRATCH / FLUTE DUST |
| Real pi32v2 | image **548,960/581,564 B** (32,604 B free), RAM **85,908/98,304 B**, pool **333,948/344,064 B**, package **610,019 B**, identity `FM-1_900`, exit 0 |
| Static cost | **33 PASS** including the new CLUSTER, existing baselines/tolerances kept, exit 0 |
| Web and latest native preview | `node web/test_web.mjs` and `tests/orbit_preview_test.py` (after building the latest C host) both exit 0. Real 44.1 kHz stereo PCM and panel/HTTP checks PASS |
| Required host checks | Linux amd64 `tools/orbit_check.py` **24/24 PASS**, exit 0. On macOS only the one IRQ check fails, from the existing Mach-O/MMIO limit, giving 23/24 PASS |
| Full Linux runner | Native ARM64 `sh tests/run_tests.sh` on a read-only copy including the real SDK and the new target outputs: **ALL HOST TESTS PASSED**, exit 0. The 108 timed CPU notes weren't used for the strict verdict, and macOS instruction-count overruns are 0 |

`build/orbit.fwsc` SHA256 before the 0.6.0 version change:
`42449d4bc553b8976d97c0c2bc743eacbb0d01ad8a5db1eb58fb8fd19b03cd44`.
Two-second example WAVs from the real C render are `build/host/lofi-flute-replacement.wav`,
`scratch-replacement.wav` and `flute-dust-replacement.wav`.

Only the three silent goldens were replaced, with reviewed new output hashes:
`6b00494918cd5c5b`, `63b3781856e18118` and `77577e91d32879d4`.
The other 207 render hashes didn't change. The normal non-silent health check was reapplied to every factory preset.
No existing CC0 file was deleted; only generated data changed.
A separate native ARM64 `orbit_check.py` run also hit the existing IRQ MMIO test failure, which is unrelated to this change.
That test doesn't include the sample outputs or audio code,
and this change neither fixed nor disabled it. The amd64 run, which includes the same test, passed in full.
Hardware install, listening, audio timing, Dots deployment and public release weren't performed.

## First official cluster integration (before the replacement sounds), 2026-10-10

This entry describes a local working tree, separate from the published and installed 0.5.0 recorded below.
CLUSTER is added as default ID 15, keeping the existing IDs 0..14 and the FUN5/int16 storage layout.
The 16 original factory names and raw knobs are preserved; pitch/ADSR/velocity/FX come from ORBIT.
Only the 54,284 B of FLUTE/SCRCH ADPCM was removed, and set/zone/preset/USR numbers are preserved.
The two SAMPLE patches using it and GRAIN FLUTE DUST are silent.

| Check | Result of a direct run |
|---|---|
| Comparison with the product C original | 729 cases, 5,103 blocks/R0, 653,184 samples and all voice/global/table state match the strict oracle. The product, const and UBSan runs and the generation reproducibility check all exit 0 |
| Real adapter, samples and transfer | 16 raw patches, 128/32 cadence, PCM/state/PRNG, release, multiple parts/arena, silence of the removed banks on all 128 MIDI notes, signed16/legacy and malformed write checks PASS |
| Strict macOS DSP regression | 210 renders, changed/gone 0, health/voice/routing/CPU overrun/crash 0, exit 0. Existing baselines/tolerances kept, only new cluster baselines added. 5 informational faster-item notes |
| Full web suite | `node web/test_web.mjs` exit 0, covering raw bounds, 16 factory rows with original names, user preset/library/lock/WATCH and legacy frame round trips |
| Native preview | `tests/orbit_preview_test.py` exit 0 after building the latest C host. Real render command, 44.1 kHz stereo and HTTP/panel checks |
| Required host checks | macOS 23/24 PASS. The existing Linux-only `irq_init_test` fails to compile because of its Mach-O section and `MAP_FIXED_NOREPLACE`. Standalone Linux gives 24/24 PASS |
| Full Linux runner | `sh tests/run_tests.sh` on a native Linux ARM64 copy of the read-only original, including the real SDK and current target outputs: **ALL HOST TESTS PASSED**, exit 0 |
| Extra FUN5 raw storage check | CLUSTER ID with COUNT 17408, WAVE -32768, CURVE 24575 and SPREAD 32767 preserved through capture/apply. The extra case ran on both macOS and Linux, exit 0 |
| Direct check in Aside | CLUSTER selection and C preview at desktop 1470px, and 32767/21120 raw display on iPhone 16 at 393px. The real C screen showed `CLUSTER grit drive`; HTTP PCM was 44100 Hz / stereo / 44032 B / peak 11863. Browser playback of the C audio starting was also confirmed |
| Real pi32v2 build | exit 0. image **539,744/581,564 B**, RAM **85,908/98,304 B**, pool **333,948/344,064 B**, package **610,019 B**, identity `FM-1_900` |
| Static target cost | **33 PASS**, the existing 31 plus the new `cls_render`/`cluster_render`, exit 0. New costs 570/570 and 53/53, 0 divides inside loops, existing baselines/tolerances kept |

`build/orbit.fwsc` SHA256 at the time:
`8aec413af72bc3315c5cee71f99985d121633a253e1812213e66bf79dc662652`.
Per-case results of the original comparison are in `build/host/cluster-product/verification.json`.
The official analysis images and the existing `.omo/evidence` were only read.
The 95 timed CPU notes from Linux ARM64 weren't used for the strict CPU verdict.
The macOS instruction-count regression above has 0 CPU overruns.
The existing favicon 404 seen in manual QA was left as is and wasn't treated as a JavaScript failure.
The working browser tabs, audio and both local servers were closed, and the original Aside tabs were restored.

The three changed existing goldens are the exact silent hash `0000000000000000` of the removed banks,
and the health check was changed to confirm that those factory banks peak at exactly 0.
No other existing golden or CPU tolerance was raised.
Reproducing the original full-patch ADSR/FX, real FM-1 audio/ISR time and cache burst load,
hardware storage and power-cycling, installation, Dots deployment and public release were unconfirmed or not performed in this work.

## ORBIT 0.5.0 final release candidate, 2026-10-10

The final candidate includes the SLOOP 2.5 integration, English public documentation and new comments, and two fixes from the release audit: FIR reads use separate pointers within the left and right history arrays; SYN backup restore sanitizes staging data before publishing the live bank with the existing void IRQ off/on pair. No buffer, format, engine ID, CPU baseline or tolerance changed.

| Check | Final result |
|---|---|
| Actual pi32v2 rebuild | exit 0. app **571,856 B**, loader binary **6,863 B**, package **610,019 B**, identity `FM-1_900` |
| ELF resources | image **571,856/581,564 B** (9,708 B free), RAM **85,892/98,304 B** (12,412 B free), pool **333,948/344,064 B** (10,116 B free) |
| Static target CPU | **31 functions PASS**, no missing baseline or overrun. FIR cost **536/536**, PHASE **78/79**, ALNK0 **138/174**, `.ram_text` 925 instructions with no calls |
| Independent Linux candidate | `tools/orbit_check.py` **21/21 PASS**, full `tests/run_tests.sh` **ALL HOST TESTS PASSED**, and latest C rebuild / HTTP / stereo / controls in `tests/orbit_preview_test.py` PASS, exit 0 |
| Sanitizer and soak | Default 15,000-frame ASan/UBSan stress, `arena_asan` with 300 simulated seconds / all 128 takeover pairs and no ownership violation, 10-minute simulated soak and integer-divide checks PASS. Default 40,000-frame stress retained in the full runner |
| Strict macOS DSP regression | Fresh `cc -O2` binary: **194 renders**, changed/gone 0, health/voice/CPU/crash failures 0, exit 0; two informational faster-item notes |
| Final-package hardware install | Write **100%**, automatic reboot and `FM-1_900` reconnection, exit 0. INFO **`FELUCCA ORBIT 0.5.0`**, protocol **10**, engines **15**, globals **33**, PING **PASS** |
| Final data preservation | Before/after the final reinstall, **14/14 backup objects byte-identical**, including SYN. The original **13/13** objects from 0.4.1 also remain byte-identical |

Final package SHA-256: `11d52a4adbe312af0eded335483628f72072cb5699ffadced56219e6d1eba3cf`. `SHA256SUMS` matches this exact file. The final fixes add 16 B to the previous candidate's image and do not change RAM, pool or FIR static cost. The final package was installed on the same FM-1 before publication.

The full Linux runner reported 22 timed CPU notes; its timed measurements are not the strict CPU verdict. The final macOS instruction counter has zero overruns against the unchanged +25% limit. Existing 64-bit host MMIO pointer and libc macro warnings, and the unchanged OTA test's possible-uninitialized warning, remain host-harness findings rather than suppressed checks.

One intermediate build failed because the new critical section was initially written for an IRQ API returning flags; the project's HAL uses a void off/on pair. The calls were corrected to the existing contract and the final target compiled successfully. All subsequent source changes were documentation only. Hardware audio quality, USB 48 kHz transfer/underrun, real ISR cycles, long-term stability, power loss/recovery and persistent SYN power-cycle remain unverified. Separate web emulator and Dots deployment are not part of this release.

Detailed terminal evidence, before/after backups and review records are retained privately under `.omo/evidence/orbit-0.5.0-release-20261010/`. These files, the user's journal and generated binaries are excluded from Git commits; only the package and checksum are attached to the public release.

## ORBIT 0.5.0 hardware installation, 2026-10-10

The user asked for installation on the device and a version bump. The source version, native preview title and web mock version were set to `ORBIT 0.5.0`, then the default target was rebuilt. When this entry was written, no public release, tag or commit had been made yet.

| Check | Actual result |
|---|---|
| `PYTHON=~/.jieli/orbit-venv/bin/python sh build.sh` | exit 0. app 571,840 B, loader binary 6,863 B / linked image 8,140 B, package 610,019 B, identity `FM-1_900` |
| Target resources and CPU | RAM 85,892/98,304 B, pool 333,948/344,064 B, image 571,840/581,564 B. All 31 `tests/target_budget.py` items PASS, exit 0 |
| Linux amd64 / Node 24 | `tools/orbit_check.py` **21/21 PASS**; `web/test_web.mjs`, `tests/orbit_preview_test.py` after a fresh C rebuild, and `tests/install_test.py` all exit 0 |
| Real web mock | `ORBIT 0.5.0 (MOCK)` shown at 1440 px and 390 px, no horizontal overflow on mobile. Kept separate from hardware validation |
| Device before install | `Felucca` port, `FM-1_900`, INFO `FELUCCA ORBIT 0.4.1`, protocol 9, engines 13, globals 32, PING OK |
| Pre-install backup | BK_LIST/BK_GET CRCs matched for 13 objects. Working project 3,840 B / CRC `0508e43c`, settings 88 B / CRC `e208a3d7`; the remaining slots, banks and USR1-4 were empty |
| Actual install | `~/.jieli/orbit-venv/bin/python tools/fm1_install.py build/orbit.fwsc --port Felucca --yes` exit 0. Loader handoff, write **100%**, automatic reboot and `FM-1_900` reconnection confirmed |
| Device after install | INFO **`FELUCCA ORBIT 0.5.0`**, protocol **10**, engines **15**, globals **33**, PING **PASS** |
| Data preservation | Post-install backup compared with the pre-install one: all **13/13 original objects byte-identical**, 0 changes. The new SYN bank object 9 is 1,464 B / CRC `41fef647` and reads back with a valid CRC |
| User confirmation | Right after installation the user confirmed normal operation; the English translation of the confirmation is "works fine". |

Installed package SHA-256: `803b61c9b8288f9b59a498deb66ae21f3ffb982226d2e2fcb9b7197da2c3174d`. Before installing, `load_package()` checked the header, file list and each file's CRC. Logs and backups were kept locally under `.omo/evidence/orbit-0.5.0-install-20261010/` (`preinstall-backup.json` and `postinstall-backup.json` use the web editor backup format). That private backup, evidence and journal material isn't part of the repository or the release. The MIDI link and the QA browser and server owned by this work were shut down.

The first `orbit_check.py` run on macOS exited 1 because the existing Linux-only IRQ harness depends on a Mach-O section and `MAP_FIXED_NOREPLACE`. Without weakening the harness, all 21 passed on Linux. The first read-only backup stopped on a CRC mismatch because the verification script confused the OTA's bitstream pack7 with the editor's MSB-group pack7. After switching to the editor scheme, every before/after CRC and byte comparison passed. This script never writes data to the device.

This installation confirmed boot of the new firmware, USB-MIDI loader transfer, reconnection, INFO/PING and preservation of existing data. Real audio quality, 44.1/48 kHz USB audio transfer, underruns, ISR cycles, long-term stability, flash power loss and recovery, and power-cycling after persistent SYN storage weren't tested separately. The separate web emulator wasn't redeployed. Statements in the 2026-10-09 entries below that the firmware wasn't installed describe the state at that time.

## SLOOP 2.5 integration working tree (undeployed at the time), 2026-10-09

The comparison runs from upstream v2.4.1 `a1c5d68767ae10fafb6821dc63b9b1fc490342d2` to v2.5 `fa9ce5742f4aea9484e7c129dd81079d07e7321f`. The ORBIT starting HEAD is `4d85cf8ae8b54059cd3b5c013561bb0d92592c23`. These values came from the then-uncommitted working tree and are separate from the 0.4.1 release and hardware entries that follow. Per-feature dispositions are in the [integration notes](SLOOP-2.5-INTEGRATION.md).

| Check | Result of a direct run |
|---|---|
| `PYTHON=~/.jieli/orbit-venv/bin/python sh build.sh` | exit 0. image **571,840 B**, loader image **8,140 B**, `build/orbit.fwsc` **610,019 B** |
| Target resources | RAM `.data + .bss` **85,892/98,304 B**, **12,412 B** free. pool **333,948/344,064 B**, **10,116 B** free. **9,724 B** free against the 581,564 B image limit |
| `python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt` | exit 0. All **31 functions** have baselines and are within budget. PHASE cost 78/79, drums_mix 6,478/6,153, ALNK0 cost 138/174 |
| macOS `cc -O2` instruction-counted regress | **194 renders, changed/gone 0, health, voice, CPU overrun and crash 0**, exit 0. Two informational notes on fast items |
| Linux amd64 / Node 24 `sh tests/run_tests.sh` | **ALL HOST TESTS PASSED**, exit 0. Default 10-minute virtual soak, 40,000 stress frames, 15,000 ASan/UBSan frames and an extra arena sanitizer run kept |
| Linux amd64 `python3 tools/orbit_check.py` | **21/21 PASS**, exit 0. The existing 19 plus arena and dsyn checks. Output in `build/host/orbit-checks.txt` |
| `node web/test_web.mjs` | `web tests passed`, exit 0. Covers v9/v10 and 32/33-global compatibility, Song, SYN, SMF, DX7 cartridge, backup/restore, package and OTA |
| `python3 tests/orbit_preview_test.py` | Builds the latest C host first. SEL, short notes, blur, pointer cancel, left/right tap from the real render command, **44.1 kHz non-silent stereo PCM** and HTTP input validation PASS, exit 0 |
| USB host checks | CDC 1/0/serial-off, all 3 PASS. 44.1/48 kHz, rate switching, ring and descriptor checks; serial-off and no-CDC descriptors identical. 1 kHz host recording simulation SNR 81.4 dB, underrun/overrun 0 |
| FIR generator | The 80x12 Q15 table generated in a temporary `uv run --no-project --with numpy` environment is byte-identical to `firmware/src/uac_fir.h`, exit 0. The default build gains no numpy dependency |
| Real browser | Chrome at 1440 px / 390 px: Sound, Sequencer, piano roll, Song, Drum synth and native C screens checked, no horizontal overflow. SYN name mock store, 32-voice DX7 file input to 27 selected, 3-note MIDI file input to an 8-step preview, native SEL click confirmed |

Package SHA-256 at the time: `8a5f06f8adfcc474ab83534b47c8601e53a76a0b26ba3cb2b5d5cfef74ab13e8`. The `FM-1_900` identity and existing install format are kept. At that point the package had been built and checked in simulation, but not installed or deployed.

Of the 105 existing golden entries, **only GM KIT** was changed on purpose, because of the generated crash/ride tail. For the 12 SWARM/PULSE/FM4 sounds that had no baseline, hashes and CPU values were taken from the preserved pre-integration binary and added only after the new renders matched 12/12 bit for bit. Baselines for the 77 new sounds and two heavy mixes were measured in ORBIT. Existing CPU values and the +25% tolerance, and existing target values and the +10% tolerance, weren't raised.

The only failure in the first integration regression (exit 1) was the intended GM KIT hash change, with 0 health, voice, CPU overrun or crash failures. After the baseline review, the same binary passed with exit 0. The 27 informational notes from Linux timed CPU weren't used for the strict CPU verdict, which came from the separate macOS instruction counter.

The final link, including FIR code and tables, runtime history and SYN state, passed every resource limit. The pitch table shrank by 7,424 B and the crash/ride generated data by 12,800 B, and pool use is 612 B lower than before the integration. No full framebuffer, PCM tape or Bluetooth was added.

The C LSP couldn't follow the unity build and generated headers, and no JS/Python language servers were installed, so verification relied on real compiles and runtime checks. Because of the existing macOS ASan allocator problem and the RAM-MMIO harness constraints, the full sanitizer and IRQ checks ran on Linux amd64. Windows `--slice 0/1` passthrough to the generator environment and WSL arguments was confirmed with a CLI mock; real Windows/WSL wasn't run. Optional SLICE engine IDs and the arena/FM6 host checks passed, but the target figures above are for the default `FELUCCA_SLICE=0` build.

Some existing async checks in `web/test_web.mjs` still use fixed `sleep` calls. This diff added 0 new sleep/timeout waits, and the native preview checks have no fixed sleep. The possible nondeterminism of the existing waits wasn't changed in this integration, and a web PASS isn't used as evidence of real-time performance or timing stability.

Unverified at the time: 44.1/48 kHz enumeration, recording and underrun on a real FM-1, ISR cycles and latency, the real-time effect of clearing the arena on engine switch, SYN flash power loss and power-cycle, and boot and installation of the new firmware. (Boot and installation were later confirmed on 2026-10-10, above.) Host torn-write and concurrent-save checks don't replace a real flash power loss. The native preview project-save is a non-persistent host double and doesn't support sample upload. Web screen checks used the mock and aren't evidence of real MIDI device transfer or hardware audio. No Dots deployment or commit was made, and user files were preserved.

## ORBIT 0.4.1 release build, 2026-10-09

Rebuilt with the default source version `ORBIT 0.4.1`. The install identity stays `FM-1_900`, and the upstream `--release X.Y` format wasn't changed.

| Check | Release result at the time |
|---|---|
| `PYTHON=~/.jieli/orbit-venv/bin/python sh build.sh` | exit 0. app 570,576 B, loader 6,863 B, `orbit.fwsc` 610,019 B |
| ELF memory and target budget | `.data + .bss` 83,972 / 98,304 B, pool 334,560 / 344,064 B. `.ram_text` 925 instructions, no calls. ALNK0 cost 138 / baseline 174 |
| Linux amd64 / Node 24 | `python3 tools/orbit_check.py` 19/19 PASS, `sh tests/run_tests.sh` **ALL HOST TESTS PASSED**, exit 0. Stress, sanitizer and soak settings kept |
| Fresh macOS `cc -O2` counted regress | 117 golden renders, changed/gone 0, health, voice, CPU overrun and crash 0, exit 0 |
| Latest native preview | C host rebuilt first, then HTTP/DOM, 44.1kHz stereo PCM and real left/right visualizer tap PASS, exit 0 |
| Install and web | Real package CRC, Python/JS logical image, loader marker and editor checks PASS in the full runner |

`orbit.fwsc` SHA-256: `31afad04333e6593e0333039c53f26b53ccab032b665cdc8e4aa3ec82bb53253`.

Logs are `build/host/release-0.4.1-build.log`, `release-0.4.1-tests.log` and `release-0.4.1-counted-preview.log`. The 21 informational notes from Linux timed CPU aren't failures; the strict CPU verdict is the separate macOS instruction counter result. The missing golden and CPU baselines for the 12 presets of the original standalone engines were left as they were.

Wasm validation found that inserting zeros between the taps `mix_block` produces only on odd samples made the lissajous draw just the center point. The browser HAL and native scope bridge now feed only the odd-sample left/right pre-master taps, as the hardware does. A real Wasm framebuffer regression with MASTER 0 and right pan failed before the fix and passed after it.
After the sampling fix, the 19 Linux host checks passed again, and the HTTP/DOM and stereo scope checks on a rebuilt native C host exited 0. The log is `build/host/release-0.4.1-sampling-preview.log`. Target firmware sources and the package SHA-256 are identical before and after this adapter fix.

Additional finding: in the native HTTP preview, pressing HOME repeatedly right after SELECT or a track switch in the Mixer during playback can leave the state in the Mixer. This isn't generalized to target or Wasm behavior, and the input scheme wasn't redesigned for this release. The Aside browser screen and Web Audio interaction for this release went unverified because macOS screen recording and accessibility permissions were denied.

The web emulator's source pin and build output are tracked in the separate [repository](https://github.com/dspaudio/orbit-web-emu) through `ORBIT_REVISION`, `build-info.json` and its validation record. FM-1 flashing, boot, USB, audio timing and Dots deployment weren't performed.

## Original OP-1 controls and fixes for existing budget failures, 2026-10-09

The reference is the [original OP-1 official guide](https://teenage.engineering/guides/op-1/original), not the OP-1 field. Following [DESIGN.md](../DESIGN.md), Synth / Drum / Event Tape / Mixer, T1 engine / T2 envelope / T3 effect / T4 LFO and the blue / green / white / orange roles were applied to the real C screens and input, the native preview and the Web MIDI editor. Existing detail pages, hold layers, palette choice, the event clipboard, storage format, device protocol and licenses were preserved. Event Tape edits sequencer events and isn't PCM recording.

### Root-cause fixes and strict budgets

- `eng_phase.c`: split the resonance window cases to drop unused window math. A comparison of 196,608 integer window expressions showed 0 differences, and the existing PHASE `03_RESO_PLUCK` golden `0f46c3e6fb6a0c15` is kept.
- `voice.c`: the overload victim search became two flat passes, cutting worst-case slot visits from 72 to 48. Release priority, tie order, POLY bass and MONO/LEGATO/UNISON lead protection and the stage-4 fade were confirmed with existing regressions. No work moved to another ISR or an unmeasured helper.
- `tests/golden.txt`, `cpu_baseline.txt`, `target_budget.txt` and their tolerances weren't changed. The earlier PHASE 1,154 / baseline 899 (+28%) failure was resolved in the final macOS instruction-counted run. The stock formatter doesn't print per-item CPU numbers for passing items, so no new figure is guessed here. Total CPU overruns are 0 with the unchanged +25% threshold.
- Final `fm1_alnk0_irq`: 221 instructions, loop 90, divide 0, loop call 2, **cost 138 / baseline 174**, down from cost 268. The full static target budget check exits 0.

### Final run results

| Check | Directly confirmed result |
|---|---|
| pi32v2 build and package | exit 0. app **570,576 B**, loader **6,863 B**, `build/orbit.fwsc` **610,019 B**, `FM-1_900` |
| Real ELF memory | `.data + .bss` **83,972 / 98,304 B**, pool **334,560 / 344,064 B**, **9,504 B** free. `.ram_text` **2,964 B**, 925 instructions, no calls |
| Linux amd64 `python3 tools/orbit_check.py` | **19/19 PASS**, including IRQ RAM-backed MMIO. `build/host/orbit-checks.txt` |
| Linux amd64 / Node 24 `sh tests/run_tests.sh` | **ALL HOST TESTS PASSED**, exit 0. Default stress 40,000 frames, ASan/UBSan 15,000 frames and the 10-minute virtual soak weren't reduced |
| Fresh macOS `cc -O2` counted regress | **117 golden renders, 0 changed/gone, 0 health/voice/CPU failures, 0 crashes**, exit 0 |
| Install and web checks | Real package CRC, loader marker, Python/JS logical image, existing SysEx, sample, backup/restore and OTA checks PASS |
| Native rebuild and HTTP/DOM checks | Real 44.1kHz stereo C PCM, state metadata, initial Tape and rapid Drum to Synth, short notes, blur, pointer cancel and left/right tap PASS. exit 0 including server cleanup |
| Real screens and browser | Real C 240x240 captures of Tape, engine, ENV, FX, LFO and mixer checked. In Chrome at 1440px / 390px DPR3 touch, real main mode and module navigation, selection display during hover and no overflow confirmed. Real Web Audio decoded 2-channel non-silent PCM |

Memory values were computed directly from the ELF's `_bss_end=01c1c804`, `_pool_start=01c20000` and `_pool_end=01c71ae0`. No full framebuffer or PCM tape buffer was added. In the mixer, SELECT opens LEVEL / PAN / the existing TRACK (SWING / selected track LEVEL / LEN / PAN), and the drum level is the real `G_DRLVL`.

### Environment limits and checks not performed

ASan in the full macOS runner failed with a `CHECK failed` during sanitizer allocator initialization itself. The same default stress was confirmed under Linux ASan/UBSan, and the final full runner also passed on Linux. The Debian default Node 18 failure on missing `deflate-raw` was fixed only by switching the environment to Node 24. Linux timed CPU is a comparison reference; the strict CPU verdict is the separate macOS instruction counter result.

The missing golden and host CPU baselines for the 12 existing SWARM/PULSE/FM4 presets were reported as is, and no new baselines were created just to make checks pass. The standalone C LSP doesn't understand the generated headers and unity build settings, and no JS/Python language servers were installed, so real compiles and runtime checks were used.

FM-1 connection, flashing, hardware boot, USB transfer, flash power loss, ISR cycles, audio underrun and latency, and Dots deployment weren't performed. The preview's project save is a non-persistent host double. The entries below are results from before the fix or from earlier versions, as of their time.

## Feature wiring, install package and branding, 2026-10-09

Environment: Apple M2 / macOS, Docker Linux amd64, `jieli-linux-toolchains-20250324.1`, AC79 SDK tag `AC79NN_SDK_V1.2.1_2023-12-13`. The SHA-256 of the SDK's `uboot.boot`, `cfg_tool.bin` and `eq_cfg_hw.bin` matched the build script's references. The Python environment is in `~/.jieli/orbit-venv`, the toolchain in `~/.jieli/toolchain` and the SDK in `~/fw-AC79_AIoT_SDK`.

| Check | Result |
|---|---|
| `PYTHON=~/.jieli/orbit-venv/bin/python sh build.sh` | PASS. Real pi32v2 compile and link plus loader and package generation |
| App / package | `build/felucca.bin` 569,600 B / `build/orbit.fwsc` 610,019 B, identity `FM-1_900` |
| Target memory | `.data + .bss` 83,972 / 98,304 B, `.pool` 334,560 / 344,064 B. `.ram_text` 925 instructions, no internal calls |
| `python tools/orbit_check.py` | 19/19 PASS on Linux amd64. Output: `build/host/orbit-checks.txt` |
| `python tests/install_test.py` | PASS. CRC, loader marker, Python/JS logical image and simulated install path of the real `orbit.fwsc` |
| `node web/test_web.mjs` | PASS. Abort after a restore data failure, original error kept when abort fails, and existing protocol, sample, FM6 and project checks |
| `python tests/orbit_preview_test.py` | PASS. Real C stereo PCM, HTTP input rejection, SCL, short key input, matching left/right taps from the real render command |
| Browser | In real WebKit: 8 English editor tabs, 1440 px / 390 px screens, `Orbit` wordmark at weight 100, non-silent 2-channel decoding from the C engine and Visualizer waveform |
| Firmware boot render | The 1-pixel `Orbit` wordmark from the real `splash.c` confirmed. Normal `ui_draw()` is delayed for at least 750 ms after `orbit_splash()`, with no new framebuffer or PCM buffer |
| Build options and file name | UAC 0/1 passthrough confirmed in real C compile arguments. Boundary check that both default and release CLIs write `orbit.fwsc` PASS |

The new regressions for panel entry, bank staging expiry and left/right taps were first reproduced failing before the fix, then passed with the same checks. Tape GLO to mixer to global settings, drum EDIT/SEQ entry, 15-second staging expiry and a real NOR write after timer wrap were confirmed. Abort after an interrupted web restore and keeping the original error also passed in the current source's tests. The generated boot capture is at `build/host/boot-qa/orbit-boot.png`.

### Existing budget failures before the fix

`sh tests/run_tests.sh` exited 1. The full log `build/host/full-tests.txt` showed only the two budget overruns below and the final failure summary; the remaining storage, recovery, sequencer, UI, DSP, USB descriptor, loader, install and web checks passed.

- Host `cpu/PHASE/03_RESO_PLUCK`: 1,154 instructions, baseline 899, +28%, allowed +25%. It was already over at 1,153 before the change.
- Target `fm1_alnk0_irq`: static cost 268, baseline 174, +54%, allowed +10%. The pre-change target build was over by the same amount.

Existing baselines and tolerances weren't changed. Only baselines for six SWARM, PULSE, FM4 and FM6 functions, measured on the target for the first time, were added so they no longer pass as `no budget`. Static instruction costs and host CPU values aren't measurements of real FM-1 cycles or audio underruns.

On macOS, `irq_init_test` doesn't compile directly because of its Mach-O section attribute and Linux `MAP_FIXED_NOREPLACE`. A Linux arm64 run didn't succeed either, so that result isn't used as evidence. The same real MMIO harness and all required checks passed on Linux amd64. The standalone C LSP doesn't understand the unity build's includes and pi32v2 settings, and no Python/JS language servers were installed, so real compiles and runtime checks were used.

FM-1 MIDI input and output weren't connected. Real device installation, boot screen transitions, USB enumeration and transfer, flash power loss, IRQ timing, audio load and latency, and Dots deployment weren't performed. Passing package generation, format and memory checks isn't taken as proof of hardware stability. Generated packages aren't committed to the repository.

The entries below are validation records from earlier versions, as of their time.

## ORBIT 0.1 validation — 2026-10-07

Host: Linux x86_64, gcc, Python 3.12. No FM-1 hardware connected.

```text
orbit_test: PASS
seq2_test: PASS
project_test: PASS
storage_test: PASS
drumkit_test: PASS
studio_drums_test: PASS
punch_test: PASS
song_audio_test: PASS
song_ui_test: PASS
```

The ORBIT test exercises event clipboard copy/lift/drop, chord/tie/dynamics preservation, destination bounds, synth/drum type rejection, real panel inputs, transport, and 2,000 UI frames with track/length changes. Framebuffer bounds are asserted.

HTTP preview integration: PASS. The native firmware generated non-silent 44.1 kHz stereo PCM through the HTTP bridge. Panel operations and invalid input rejection were checked. Browser JavaScript passed node --check. A full browser audio session was not exercised.

The full firmware translation unit passed preprocessing with its HAL and generated headers. Target package build was attempted and stopped because the AC79 SDK files were missing. The pi32v2 toolchain is also absent; its vendor endpoint timed out. No installable .fwsc is included. Target binary size, BSS, IRQ timing, USB hardware and Bluetooth were not tested.

Existing web editor suite: PASS (sample byte parity, import/CHOP, backup/restore, protocol, mock upload and OTA error paths). Package tests were skipped because no target .fwsc exists. Native host compilation produces existing HAL pointer-width warnings on x86_64; no warning originates in the ORBIT additions.

## 0.2 graphics update

All nine host test programs passed after the UI changes. Tape, all engine pages, envelope, LFO, mixer and sample output rendering were exercised. The host preview now writes its actual mixed PCM into the scope ring, matching the hardware scope source. The native HTTP preview passed again, and firmware preprocessing passed. Target toolchain/SDK and hardware validation remain unavailable.

## 0.2.1 IRQ initialisation mitigation

The real fm1_irq_init now explicitly clears EMU_CON bit 2 instead of setting it. The Linux RAM-backed MMIO test passes with zero, bit-2-set, all-bits-set and mixed initial states; unrelated EMU bits, branch trace, exception configuration and vectors are preserved, and a second initialisation remains safe. This is a host register-write test, not a target compiler or FM-1 hardware test. The existing recovery test and all nine functional test programs passed. Firmware preprocessing passed. Target compilation and hardware validation remain pending.

Sequence screenshots are rendered by the actual STEP and PATTERN UI with a representative 16-step pattern and a chord. The screenshot fixture does not change firmware boot defaults.

## HOME preset feedback correction

All 68 factory browser entries resolve to a real engine preset. Native panel tests verify that consecutive and multiple PRESETS detents load individual bass sounds and that HOME feedback matches the actual selected engine/preset. Tape layout is unchanged.


## ORBIT 0.3.0 independent synthesis and FIRST LIGHT

- All 12 host programs pass, including the new independent DSP suite.
- All twelve new patches: deterministic direct renders, MIDI 24–120, parameter min/max, output bounds and guards. FM4 depth-zero carrier: 440 Hz.
- New engine suite passes with optional SLICE enabled and under undefined-behaviour sanitisation. Engine voice state remains 84 bytes.
- Actual menu: initial demo confirmation preserves the working song, cancel clears it, second confirmation loads all three new engines and four-bar patterns without saving a numbered slot. Loading while playing is rejected.
- FIRST LIGHT actual C sequencer rendered 12 seconds of non-silent stereo PCM; MP3 is a listening copy. Screens are captured from the actual C framebuffer.
- Native HTTP preview test and complete firmware preprocessing pass.
- No pi32v2 build, hardware boot, target linker budget or IRQ timing measurement is available.


## ORBIT 0.3.1 LFO correction

LFO source controls have no audio effect while all destination depths are zero; this is intentional and preserves patch defaults. SOURCE/DEST page labels and a zero-depth hint make that condition visible. New-engine SHP destinations now control harmonic balance, pulse width and FM4 depth.

The added actual voice/LFO PCM regression suite verifies that zero-depth renders remain identical when source controls change, that PIT/FLT/SHP/AMP each change PCM on all three new engines, and that RATE/WAVE/PHASE/FADE each change PCM when a destination is enabled. Source-phase and fade tests retrigger a fresh phrase. UI regression checks the second LFO press and KNOB4 destination-depth update. All 13 host programs pass.

## ORBIT 0.4.0: SLOOP 2.4.1 and color styles

Upstream integration source: v2.4.1 / a1c5d68767ae10fafb6821dc63b9b1fc490342d2. Issue #1 is a display/update proposal; the user's follow-up selects optional color styles instead of mandatory monochrome.

- 19 host programs: FM6 (including AMS, all 32 algorithms, envelopes, patch formats and NOR bank), user kits / USR4, stress, UI pages, UAC, user presets, ORBIT DSP/LFO, IRQ mitigation, recovery, event Tape, sequencer, project migration, storage, drums, recording, song audio and UI.
- Existing ORBIT FUN4 engine IDs retained through FUN5 conversion; optional SLICE build retains ORBIT ordering and passes independent-engine checks.
- Tape metadata: nudge, fill and locks survive copy/drop; lift clears metadata; lock-capacity overflow refuses a drop before editing the destination.
- Actual framebuffer captures of ORBIT, PASTEL, NEON and MONO are in docs/orbit-styles-0.4.png. They are firmware renders, not hardware photos.
- Native HTTP preview: 44.1 kHz stereo, finite non-silent sequence audio, panel operations and invalid input rejection.
- Web editor checks: protocol v9, dynamic FM6 engine ID 12, DX7 voices/banks, sample and kit packing, locks/nudges/fills, backup/restore and OTA simulations. Package verification is skipped without a target package.
- The wasm tests exercise eight FM6 factory sounds, the twelve independent patches, FIRST LIGHT, all twelve independent-engine/LFO destination combinations, rapid button queues, autosave reboot, color-style persistence and visualizer selection.
- Memory / divide stress uses AddressSanitizer and integer-divide-by-zero checks for 6000 frames. Leak detection is disabled because the managed process environment prevents LeakSanitizer's /proc inspection; firmware buffers are static.

A broader undefined-behavior sanitizer trial reports inherited signed-left-shift expressions in GRAIN. ORBIT does not claim whole-tree UBSan cleanliness. Negative shifts observed in FM6 velocity/LFO and common DSP DC/chorus/filter paths are expressed as multiplication, preserving intended arithmetic.

The pi32v2 target compile, target RAM/flash budget, IRQ timing, USB enumeration, transfers and real-device boot/audio remain unverified. No installable ORBIT package is shipped. The hardware div0-trap clearing mitigation remains present and host tested.
