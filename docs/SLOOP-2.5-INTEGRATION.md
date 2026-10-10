# SLOOP 2.5 into ORBIT

This page records the 2026-10-09 integration and the 2026-10-10 installation and release of ORBIT 0.5.0. It's separate from the features and hardware validation of the earlier ORBIT 0.4.1 package. We compared the upstream [v2.5 release](https://github.com/isod89/sloop-fm1/releases/tag/v2.5) and the [v2.4.1...v2.5 changes](https://github.com/isod89/sloop-fm1/compare/v2.4.1...v2.5). The public package and checksum live in the [0.5.0 release](https://github.com/dspaudio/orbit/releases/tag/v0.5.0), and the summary is in the [release notes](releases/0.5.0.md).

On 2026-10-10, at the user's request, the displayed version of the firmware, native preview and web mock was raised to **ORBIT 0.5.0**. The package file name `orbit.fwsc` and the install identity `FM-1_900` are unchanged. For installation status, package hash and device INFO/PING results, the latest entry in the [validation record](VALIDATION.md) is authoritative.

ORBIT 0.5.0 is installed on the user's FM-1. The write reached 100%, the device rebooted on its own, and it answered INFO as `FELUCCA ORBIT 0.5.0` with protocol v10, 15 engines and 33 globals, plus a working PING. All 13 objects of the original backup were bit-identical before and after installation, and the user confirmed normal operation. Statements below about the firmware being uninstalled or unverified on hardware describe the state as of the 2026-10-09 integration. Audio timing, real 48 kHz transfer and long-term stability are still unverified. Private backups, device data and internal work journals aren't part of the release.

- Previous upstream: `a1c5d68767ae10fafb6821dc63b9b1fc490342d2`.
- v2.5: `fa9ce5742f4aea9484e7c129dd81079d07e7321f`.
- ORBIT starting HEAD: `4d85cf8ae8b54059cd3b5c013561bb0d92592c23`.
- All 72 changed upstream paths were classified. The table below groups them into 20 user features and fixes; the original paths are visible in the upstream compare above. A private internal audit journal records, per path, whether it was applied, already covered or excluded, and why.

## Applied, already covered, excluded

| Change | ORBIT disposition |
|---|---|
| Small pitch LUT | Applied. The 8,192 B table is replaced by a 768 B top-octave table and `pitch_inc()`. All 2,048 pitch values are identical and existing renders are preserved |
| Shared engine arena | Applied. GRAIN/FM6/PHYS share a per-part arena. Switching waits for the old engine's fade to finish, with no separate PCM buffer |
| PHYS | Applied. MODAL/STRING/MEMB/SYMP, 26 sounds. SYMP uses 2 voices, the rest 3, and the overall 8-voice synth budget holds |
| NOISE | Applied. 11 sounds, state is kept across retriggers |
| New presets for existing engines | Applied. 40 appended across 8 engines. With PHYS/NOISE that's 77 new sounds and a 165-sound default factory bank. The first 12 ORBIT sounds and existing preset indexes are unchanged |
| GM cymbal tail | Applied. Crash/ride are generated at 0.6 s with a long fade. This frees 12,800 B of generated data; the resulting GM KIT hash change was reviewed separately |
| Web LEN/piano roll/MIDI | Applied. LEN 1-64, note/chord entry, deletion and length editing, MIDI import preview, export and GM drum mapping. The 64-step firmware base was already covered |
| Web Song | Applied. Edits the arrangement in existing settings object 1. Offset 48, 36 B size checked by compile assert, reordering during playback is refused |
| USB audio 48 kHz | Applied. The engine stays at 44.1 kHz and a FIR feeds 48 kHz host streams. The existing 512-frame ring is kept |
| SYN1-SYN4 | Applied. Four user synth drum kits, 16 sounds per kit and 22-byte sound records, with audition, editing, storage and backup object 9 |
| DX7 cartridge batch store | Applied. Pick up to 27 voices from a 32-voice SysEx, write bank object 8 once and optionally register user presets. The firmware bank base was already covered |
| Drum delay send | Applied. `G_DRDLY=32` reuses the existing delay buffer. It's stored in a FUN5 reserved byte, so the saved globals array stays at 32 |
| MIDI CC | Applied. Mapped to the channel's track; drum CC 7/91/94 control LVL/REV/DLY. With IN=CLOCK, CC is ignored |
| Swing display | Applied. Shown as 0-100; stored values and sequencer timing are unchanged |
| SCL to SEL | Applied. Only the user-facing button, page and preview names change. `B_SCL`, `FAM_SCL` and the scale parameter `SCL` stay |
| Drum level dial | Applied. The ghost, soft, norm, hard order and positions now share the existing `lvl_rank()` |
| DX7 AMS noise fix | Already covered. The v2.4.1 code and regression test were already integrated |
| Independent metronome/count-in | Applied. A click voice that USR/SYN kits and drum mute/solo don't affect |
| STEP chord capture | Applied. Records the real CHORD/CHORD+/VLEAD result instead of just the root, and MONO behaves as before |
| Windows SLICE passthrough | Applied. `tools/build_windows.py --slice 0/1` reaches both the generator and the WSL compile environment |

We excluded upstream-only `build-sloop.ps1`, `web/index_pkg.html`, `DEMARRAGE-RAPIDE-FR.md`, a verbatim copy of `SLOOP.md`, two generated `docs/webapp` copies and two upstream `.fwsc` release files. ORBIT has no matching installer or site pipeline, and the rest are product-named docs or release artifacts. The feature sources went into the ORBIT build and user docs, and a fresh ORBIT package was built. The source version and wordmark weren't renamed to SLOOP. CC0 sample and asset attribution and the GPL, Apache (msfa) and DaisySP/Rings MIT notices are kept.

## Usage

- **New sounds:** the first 12 PRESETS are still the ORBIT sounds. Pick PHYS/NOISE and the added presets from the full bank or from the engine in the web Sound view. Default IDs are FM6=12, PHYS=13, NOISE=14; with optional SLICE each is +1.
- **SYN:** choose SYN1-SYN4 at the end of the drum track KIT list. In the web **Drum synth** view, pick a kit and sound, edit values or Copy an existing synth kit. **Play** auditions, **Use on the drum track** selects it, and **Store on the FM-1** saves all four kits together with the settings. Stop playback before storing. Save/Open file and backup/restore are supported too.
- **Drum delay:** set the send with the fourth knob, **DLY**, on the GLO DRUMS page, or with MIDI CC94. At 0 you get the old dry/reverb behavior.
- **Piano roll/MIDI:** in the web Sequencer, choose Length 1-64 and **Piano roll**. MIDI import first shows tracks, grid, length and conversion warnings, then applies on Import. Synth note lengths become ties; drum notes map to GM lanes and levels.
- **Song:** in the web Song view, edit up to 16 sections A-D with bars/repeats, order and loop, then send them to the device. Play them in song mode with SAVE+key 13 on the device. Arrangement changes are refused during playback.
- **DX7:** pick FM6 in Sound and open a cartridge with Import SysEx or by dropping a `.syx`. Choose up to 27 voices and run **Store cartridge in bank**. Read the bank overwrite warning and, if you want, register user presets too.
- **Labels:** `SEL` is the old key/chord button and page. Don't confuse it with the `SELECT` knob. The scale parameter is still `SCL`. Swing 0 is straight and 100 is the maximum.
- **USB audio:** the host picks 44.1 or 48 kHz. The synth engine and native preview PCM stay at 44.1 kHz. For real device compatibility and latency, see the unverified scope below.

| MIDI CC | synth track | drum track |
|---:|---|---|
| 5 | GLIDE | ignored |
| 7 | LEVEL | DRUMS LVL |
| 10 | PAN | PAN |
| 71 | engine RES/Q, ignored if absent | ignored |
| 72 / 73 / 75 | release / attack / decay | ignored |
| 74 | track FILTER | FILTER |
| 91 | reverb send | DRUMS REV |
| 93 | chorus send | ignored |
| 94 | delay send | DRUMS DLY |

Channels 1-3 control their synth tracks, the configured drum channel controls the drum track, and any other channel controls the selected track. Values 0-127 map to the parameter range; for bipolar PAN/FILTER, 64 is center. With IN=CLOCK, note-ons and CC are ignored while existing note-offs still apply.

## Storage, compatibility and resources

FUN5 stays at **3,840 B**. Migration from old FUN1-FUN4 and round trips of existing ORBIT FUN5 files were tested. There are 33 runtime globals but 32 file globals; drum delay lives in a reserved byte. Settings and the SYN bank are saved as one A/B unit, so a partial save or an old-style settings save can't wipe SYN. The editor protocol is **v10**, with new commands 72-76. The web editor checks capabilities on older v9/32-global devices and limits new features there. Raw SLOOP projects/backups and raw ORBIT images use different engine registries, so we don't assume they're compatible. No support was added for old ORBIT or SLOOP 2.3 reading FUN5.

The final default pi32v2 build has image **571,856/581,564 B**, RAM **85,892/98,304 B** and pool **333,948/344,064 B**. Compared with before the integration, that's image +1,280 B, RAM +1,920 B and pool -612 B. Freeing 20,224 B of generated pitch/sample data and sharing the arena is what lets the new features link within limits. The existing PHASE/voice overload optimizations and the CPU +25% and target +10% tolerances are unchanged.

**Validation:** passed 21/21 host checks, the full host, ASan/UBSan and soak runs, 194 golden/counted CPU renders, 31 target budget items, and native C preview plus web protocol, file and backup checks. The 12 existing ORBIT sounds are bit-identical to the pre-integration binary. Only one intended GM KIT hash changed, and only new entries were added to the baselines. Detailed numbers, commands and environment limits are in [VALIDATION.md](VALIDATION.md).

**Unverified as of the 2026-10-09 integration:** installing and booting the new firmware on hardware, USB 44.1/48 enumeration, recording and underrun, real ISR cycles, the real-time cost of clearing the arena, flash power loss and power-cycle, real Windows/WSL, and optional SLICE target resources. At that point nothing had been committed, installed on hardware or deployed to Dots. Installation, boot and INFO/PING were later confirmed on 2026-10-10, as described at the top of this page. Everything else on that list is still unverified, and Dots deployment still hasn't happened. The native preview project-save is a non-persistent double and doesn't support sample upload. Event Tape edits sequencer events and isn't audio recording. Bluetooth, PCM tape and a full framebuffer weren't added.
