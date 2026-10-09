# ORBIT

**An experimental groovebox firmware for the FM-1, built on SLOOP/Felucca.**

ORBIT combines the existing synth, sampler and sequencer with an event Tape workflow and original graphics inspired by the OP-1's four-colour controls. Current development version: **0.4.0**.

> Development status: host tests pass, but the pi32v2 target build and real FM-1 validation are pending. No installable firmware package is provided. Bluetooth headphone pairing and audio output are not implemented.

## Color styles and SLOOP 2.4.1 integration (0.4.0)

Issue [#1](https://github.com/dspaudio/orbit/issues/1) proposed a cleaner display and newer SLOOP/Felucca features. ORBIT keeps its four-color identity and makes monochrome optional. Hold HOME, leave it released, select **SCREEN** with SELECT, and turn **KNOB1 / STYLE**. **ORBIT** is the fresh-device default; **PASTEL**, **NEON**, **MONO**, GREEN, AMBER, CYAN and RED are available. Track and knob colors change together; labels, track numbers and position markers remain visible in every style. Close with OCT− to save the setting. Existing saved style selections are preserved.

![Actual firmware color styles](docs/orbit-styles-0.4.png)

Integrated from upstream tag **v2.4.1**, commit `a1c5d68767ae10fafb6821dc63b9b1fc490342d2`:

| Feature | ORBIT 0.4.0 status |
|---|---|
| FM6 | Six operators, 32 algorithms, eight factory patches, 27 bank slots; 2.4.1 AMS fix included |
| DX7 patches | Single-voice / 32-voice SysEx import and operator editing through the Web MIDI editor |
| Sequencer | 24 parameter locks per track, micro timing (−32…31), fill / no-fill conditions, longer step divisions |
| Chords | Live chord modifiers, inversions, ±60 ms strum and voice leading |
| Performance | Quick pattern chains, LP/HP track filter, dotted delays, MIDI SEQ output and clock-only input |
| Samples and drums | USR4 and four user drum-kit slots; combined USR3+4 kits through the editor |
| Display and controls | 12 audio visualizers, SELECT page navigation, larger readouts, grouped settings and ALL KEYS lighting |
| Editor | Updated pixel interface, drum-kit builder, locks, nudges, fills and FM6 patch tools |
| USB | Upstream USB SERIAL default OFF and count-in / note-off fixes |

Tap HOME while on the event Tape to open the visualizer; turn SELECT for its style, then tap HOME to return. SAVE tapped from HOME opens SONG; from an editing page it opens SAVE / PRESETS. Hold SAVE for the section/chain layer. The first twelve PRESETS entries remain the independent ORBIT sounds; the complete factory bank now has 88 entries.

ORBIT engine IDs 0…11 remain unchanged; FM6 is appended as **12** (13 with the optional SLICE engine). Existing ORBIT 0.3 FUN4 projects migrate to FUN5 while retaining engine IDs, notes and sound parameters. Tape COPY / LIFT / DROP also carries nudges, fill conditions and remapped parameter locks. A drop that would exceed the 24-lock limit fails without changing the destination. The inherited undo buffer covers note events; use the clipboard to restore lifted step metadata.

**Save a backup before downgrading:** FUN5 files cannot be loaded by ORBIT 0.3 / SLOOP 2.3. SLOOP 2.4 uses a different engine registry (FM6=9), so its raw project/backup images are not interchangeable with ORBIT 0.4. The editor discovers engine IDs by name for patch-library conversion. USB/editor transfers and custom sample uploads are implemented for the firmware but remain unavailable in the emulator HAL.

## Independent sound engines (0.3.0)

ORBIT now has independently written SWARM (six detuned harmonic oscillators), PULSE (dual pulse oscillators with PWM and edge correction), and FM4 (four sine operators with three routing choices). These are original implementations of synthesis families documented for the original OP-1, not ports of Teenage Engineering algorithms or factory sounds.

The first 12 PRESETS entries are new ORBIT sounds. New/empty projects start with FM4 / ORBIT ROUND, SWARM / ORBIT HAZE and PULSE / ORBIT PWM. Existing saved projects retain their legacy engines and sound parameters.

![Independent ORBIT engine parameter screens](docs/orbit-engines-0.3.png)

[Listen to the C DSP demonstration](docs/audio/orbit-engines-0.3.mp3): SWARM / ORBIT HAZE, PULSE / ORBIT PWM, then FM4 / ORBIT TINES (three seconds each). This is host-rendered audio, not a recording of OP-1 or FM-1 hardware.

Read [engine feasibility, controls and limitations](docs/OP1-ENGINES.md) for what is implemented, deferred and unknown. The voice allocator, envelope, mixer, effects, sample assets and sequencing infrastructure remain derived from SLOOP/Felucca. The new oscillator algorithms and twelve patch definitions are in `firmware/src/eng_orbit.c`.

## LFO controls (0.3.1)

LFO has two pages. **LFO SOURCE 1/2** controls RATE, WAVE, PHASE and FADE; these define the modulation signal, not its strength. Press LFO again for **LFO DEST 2/2**: PIT (pitch), FLT (tone/filter), SHP (engine shape) and AMP (amplitude) depths. A source with all four depths at zero does not change the sound. The first page displays `NO DEPTH: PRESS LFO` in that case.

For a clear first test, set LFO DEST KNOB4 / AMP to about 50%, then return to SOURCE and adjust RATE and WAVE while holding a note. PHASE is the starting phase of a new phrase; release all notes and retrigger to hear its change. FADE introduces modulation gradually after retrigger.

SHP now modulates harmonic balance in SWARM, pulse width in PULSE, and operator modulation depth in FM4. Patch defaults remain unmodulated unless a destination depth is enabled.

![LFO source and destination pages](docs/orbit-lfo-0.3.1.png)

## Built-in demo song: FIRST LIGHT

Load it through **hold HOME → SELECT: SYSTEM → PRESETS: DEMO SONG → OCT+ → OCT+ again**, then press **PLAY**. Stop playback first. The first confirmation shows AGAIN; OCT− cancels. Loading replaces the current working patterns and sounds, so save work you want to keep beforehand. Numbered project slots are not written by the loader; normal working-project autosave still applies.

FIRST LIGHT is an original four-bar, 64-step loop at 108 BPM: Am7 → Fmaj7 → Cmaj7 → G7. It combines FM4 / ORBIT ROUND bass, SWARM / ORBIT HAZE sustained chords, PULSE / ORBIT PIN melody and the existing synthesised 808 drums. The notes, dynamics, chords and ties remain fully editable. It is not a prerecorded backing track or a multi-section arrangement.

[Listen to FIRST LIGHT](docs/audio/orbit-first-light.mp3) (actual C sequencer/DSP render).

![FIRST LIGHT event Tape](docs/orbit-demo-0.3.png)

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
| Tape editing | Implemented; host tested | Preserves chords, ties, velocity, ratchets, nudges, fills and locks; clips at the pattern boundary and checks synth/drum compatibility |
| Synths | Three independent ORBIT engines added | SWARM, PULSE and FM4, with 12 original presets; nine legacy engines retained for compatibility |
| Sampler | Existing functionality retained | Four user slots; sample import, CHOP and device upload through the original web editor |
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

HOME now briefly displays the loaded engine and preset after turning PRESETS. It browses individual sounds rather than only the first sound of each category.

![HOME preset selection feedback](docs/orbit-preset-feedback.png)

## Development milestones

- **0.1:** Added event Tape editing, native firmware rendering and the browser preview.
- **0.2:** Added reel, oscillator/orbit, envelope and mixer graphics; connected the preview scope to the actual mixed audio.
- **0.2.1:** Applied the divide-by-zero trap mitigation and captured STEP/PATTERN screens.
- **0.3.0:** Added independent SWARM, PULSE and FM4 engines, 12 original patches and independent defaults; verified native DSP and wasm32 integration.
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
| PRESETS | Browse individual sounds; HOME briefly shows the loaded engine and preset |
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

The 0.4.0 host checks passed **19 test programs**, covering IRQ initialisation, recovery, Tape editing, panel input, sequencer behaviour, projects, storage, drums, recording, song audio and UI bounds. Native HTTP preview integration, non-silent 44.1 kHz stereo output, input rejection and JavaScript syntax checks passed during preview development. The existing web editor suite also passed; package checks were skipped because no target firmware package exists.

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

Based on [isod89/sloop-fm1](https://github.com/isod89/sloop-fm1), originally commit `d691ba7b2d922f1a1f41a3622cffe29ce41c5506` (v2.3), with the v2.4.1 changes integrated in ORBIT 0.4.0. This is an independent repository created from a pinned source snapshot, rather than a fork containing the full upstream Git history.

Software is **GPL-3.0-only**. Original copyright notices and licence files are retained. Fonts and sample assets have their own licensing terms; see [LICENSING.md](LICENSING.md) and the notices in the asset directories. The original project documentation is preserved in [README-SLOOP.md](README-SLOOP.md).
