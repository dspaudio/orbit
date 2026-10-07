# ORBIT

**An experimental groovebox firmware for the FM-1, built on SLOOP/Felucca.**

ORBIT combines the existing synth, sampler and sequencer with an event Tape workflow and original graphics inspired by the OP-1's four-colour controls. Current development version: **0.2.1**.

> Development status: host tests pass, but the pi32v2 target build and real FM-1 validation are pending. No installable firmware package is provided. Bluetooth headphone pairing and audio output are not implemented.

## Screens

These images are rendered by the actual firmware UI in the native host harness, using representative test patterns. They are not photographs of an FM-1 or design mockups.

![Tape, synth, envelope, mixer and sampler screens](docs/orbit-preview-0.2.png)

### Step sequencer

The existing SLOOP piano-roll sequencer is retained. The capture below shows a 16-step pattern with a chord, note parameters and an accent.

![Step sequencer](docs/orbit-sequencer.png)

### Pattern view

![Pattern screen](docs/orbit-pattern.png)

## Features and current progress

| Area | Status | Details |
|---|---|---|
| Event Tape | Implemented; host tested | Four-track event timeline, transport position, inclusive region selection, COPY / LIFT / DROP |
| Tape editing | Implemented; host tested | Preserves chords, ties, velocity and ratchets; clips at the pattern boundary and checks synth/drum compatibility |
| Synths | Existing functionality retained | Nine engines with sound, envelope, LFO and FX editing |
| Sampler | Existing functionality retained | Three user slots; sample import, CHOP and device upload through the original web editor |
| Sequencer | Existing functionality retained | Up to 64 steps, live recording, chords, ratchets, drum lanes and song arrangement |
| Graphics | Implemented; host tested | Two Tape reels, coloured synth graphics, envelope curves and four mixer faders |
| Mixer | Existing functionality retained | Track levels, pan, mute, solo and effects |
| Project storage | Existing format retained | FUN4 projects; Tape selection and clipboard are temporary |
| Native preview | Implemented; host tested | Browser controls backed by the firmware's actual C DSP, sequencer and UI |
| Divide-by-zero trap mitigation | Applied; host register test passed | Explicitly clears EMU_CON bit 2 during IRQ initialisation |
| Target firmware build | Pending | Requires the JieLi pi32v2 toolchain and AC79 SDK |
| Real FM-1 validation | Pending | Boot, memory budget, IRQ timing, controls, USB and sample upload still need hardware checks |
| Bluetooth headphones | Not implemented | Pairing, reconnect and Bluetooth audio require further SDK integration and device testing |
| PCM audio Tape | Not implemented | Tape edits note/drum events; it does not record long audio or provide tape-speed pitch changes |

The SAMPLE/GRAIN scope displays the actual **master audio output**, rather than a raw sample waveform for trim editing. Synth graphics visualise parameters. ORBIT uses original graphics and does not include OP-1 artwork, audio or logos.

## Development milestones

- **0.1:** Added event Tape editing, native firmware rendering and the browser preview.
- **0.2:** Added reel, oscillator/orbit, envelope and mixer graphics; connected the preview scope to the actual mixed audio.
- **0.2.1:** Applied the divide-by-zero trap mitigation and captured STEP/PATTERN screens.
- **Next:** Complete target compilation and memory/timing checks, then validate on FM-1 hardware. Bluetooth support remains a separate development task.

## Try the native preview

Requirements: Python 3, a C compiler (`gcc` or `cc`) and Pillow. Node.js is used for browser script syntax checks.

```bash
python -m pip install -r requirements.txt
python tools/orbit_preview.py --port 8080
```

Open http://127.0.0.1:8080 and press the audio-start button. The preview includes a demo pattern; the firmware boot project does not.

The preview uses the firmware DSP but buffers approximately 250–550 ms of browser audio, so it cannot establish device performance or playing latency. Its project save/load doubles are nonpersistent. Uploading user samples into the preview process is not supported.

## FM-1 controls

| Control | HOME / event Tape action |
|---|---|
| KNOB 1 | Set the paste destination (head) |
| KNOB 2 / 3 | Set region start / end, including both endpoints |
| KNOB 4 | Select COPY or LIFT |
| OCT− | Copy or lift the selected region |
| OCT+ | Drop the clipboard at the head; overwrite destination events |
| ALGORITHM | Select a track |
| SELECT | Adjust BPM |
| PLAY / REC | Existing transport / live recording |
| EDIT / SEQ / GLO | Sound editing / step sequencer / mixer |
| Hold HOME | Existing settings menu |

Synth clips can move between synth tracks. Drum clips can only be dropped onto the drum track. Synth LIFT participates in the existing EDIT+OCT− undo path. A lifted drum region can be restored by dropping the clipboard; copying another region replaces that clipboard. Outside HOME, the existing OCT± controls remain available.

For sample import and CHOP editing, use `web/editor.html`. Device communication uses the original SLOOP Web MIDI workflow and browser requirements; see [the upstream README](README-SLOOP.md).

## Validation

```bash
python tools/orbit_check.py
python tools/orbit_preview.py --port 8080
# Stop the preview after its first native build, then run:
python tests/orbit_preview_test.py
```

The 0.2.1 host checks passed **11 test programs**, covering IRQ initialisation, recovery, Tape editing, panel input, sequencer behaviour, projects, storage, drums, recording, song audio and UI bounds. Native HTTP preview integration, non-silent 44.1 kHz stereo output, input rejection and JavaScript syntax checks passed during preview development. The existing web editor suite also passed; package checks were skipped because no target firmware package exists.

See [validation details](docs/VALIDATION.md) for evidence and limitations. Host results do not establish target boot safety, RAM/flash usage or real-time audio performance.

### Boot exception mitigation

Following the upstream Felucca #61 / #111 mitigation, `fm1_irq_init` explicitly clears EMU_CON bit 2, including a stale setting from a previous boot. Other register bits, exception configuration, vectors and branch tracing are preserved.

The test runs the actual initialisation function against RAM-backed MMIO on Linux and checks cold/warm states and repeated initialisation. Disabling the trap does not fix arithmetic errors or guarantee safe boot on hardware; target compiler and device validation are still pending.

## Build for FM-1

Follow [BUILDING.md](BUILDING.md) for the JieLi pi32v2 Linux toolchain and AC79 SDK requirements.

```bash
export JIELI_TOOLCHAIN=/path/to/jieli/toolchain
export AC79_SDK=/path/to/AC79_SDK
python tools/build.py
```

The inherited output filename is `build/felucca.fwsc`. No such installable package is included in this repository. The target toolchain and SDK were unavailable in the development environment.

## Origin and licensing

Based on [isod89/sloop-fm1](https://github.com/isod89/sloop-fm1) at commit `d691ba7b2d922f1a1f41a3622cffe29ce41c5506`. This is an independent repository created from a pinned source snapshot, rather than a fork containing the full upstream Git history.

Software is **GPL-3.0-only**. Original copyright notices and licence files are retained. Fonts and sample assets have their own licensing terms; see [LICENSING.md](LICENSING.md) and the notices in the asset directories. The original project documentation is preserved in [README-SLOOP.md](README-SLOOP.md).
