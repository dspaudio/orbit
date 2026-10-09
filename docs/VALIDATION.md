# ORBIT 0.1 validation — 2026-10-07

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
