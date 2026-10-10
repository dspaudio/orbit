# ORBIT validation record

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
| User confirmation | Right after installation the user confirmed normal operation (in Korean, **"잘 되네"**, "works fine") |

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
