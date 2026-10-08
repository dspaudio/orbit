# Original OP-1 synthesis feasibility and ORBIT 0.3.0

This analysis concerns the **original OP-1**, not OP-1 Field. Primary references:

- https://teenage.engineering/guides/op-1/original/synthesizer-mode
- https://teenage.engineering/guides/op-1/original/reference

These describe synthesis families and controls, but do not provide the DSP implementation, exact parameter transfer functions or the complete factory sound definitions. An independent synthesizer of the same family is feasible; bit-identical sound reproduction is not established by those descriptions. ORBIT includes no Teenage Engineering firmware, artwork, factory patches or recordings.

## Feasibility decisions

| Original engine | Independent implementation on this architecture | ORBIT decision |
| --- | --- | --- |
| Cluster | Up to six oscillator layers are practical without delay buffers. Exact chaining, envelope and unitor behaviour is unknown. | Implemented **SWARM**: six detuned sine/harmonic layers, envelope-controlled spread. Different parameter model. |
| Pulse | A dual pulse oscillator, PWM and tone control are practical. The original modulation/filter details are unknown. | Implemented **PULSE**: dual pulses, duty-cycle DC correction, polynomial edge correction, PWM oscillator and one-pole tone control. |
| FM | Four sine operators and a small routing matrix are practical without added voice RAM. Exact OP-1 topology and scaling are unknown. | Implemented **FM4**: chain, parallel pairs and fan-in routes, integer ratios, envelope depth and feedback. |
| Phase | Independent phase distortion is practical. | Deferred: legacy PHASE already supplies this family; a replacement requires a distinct implementation and listening comparison. |
| Digital | Wave shaping, detune, ring modulation and quantisation are practical. | Deferred: possible, but the original wave/shaper/digitalness mapping is unspecified. |
| DSynth | Two oscillators and separate modulation envelopes are practical. | Deferred: separate envelope states and modulation routing need further work. |
| String | A waveguide / Karplus–Strong model is feasible in principle. | Deferred: per-voice delay storage, fractional tuning, damping and target timing require a memory budget. At 44.1 kHz, a 55 Hz string needs about 802 samples, approximately 1.6 KB per 16-bit delay, before any extra states. |
| Dr. Wave | The reference describes frequency-domain synthesis; the overview also mentions an 8-bit character. Those labels alone do not specify an algorithm. | No exact reproduction planned. A future spectral/lo-fi engine would be an approximation. |
| Voltage | Public controls imply electric synthesis, shaping and detuning, without an implementation. | No exact reproduction planned; an independent nonlinear oscillator engine remains possible. |
| DNA | CPU-ID noise synthesis is device-specific and underspecified. | No exact reproduction planned. Seeded procedural noise is possible but would have different behaviour. |
| Synth / drum samplers | Sample playback, trimming and user samples are practical; ORBIT already retains this infrastructure. | Retained. Factory OP-1 samples and direct OP-1 patch import are not provided. |

The feasibility judgments above are engineering inferences from the public descriptions and current source architecture, not hardware measurements.

## Implemented controls and patches

All three engines have two EDIT pages and a four-voice per-track cap, sharing the existing eight-voice global budget. First-page knobs:

| Engine | Blue | Green | White | Orange |
| --- | --- | --- | --- | --- |
| SWARM | Oscillator count (1–6) | Frequency spread | Second harmonic mix | Envelope influence on spread |
| PULSE | Duty width | Second oscillator detune | PWM depth | PWM rate |
| FM4 | Phase modulation depth | Operator 2 ratio | Operator 3 ratio | Operator 4 ratio |

Second page: SWARM tone; PULSE tone and oscillator mix; FM4 tone, routing, envelope depth and feedback. Spread/detune units are engine-specific linear frequency offsets, not OP-1 units or cents. Shared ADSR, LFO, sequencer and effects remain available. Pitch, amplitude and tone modulation are supported; the new engines do not implement the shared LFO's shape destination yet.

Original factory patch names:

- SWARM: ORBIT HAZE, ORBIT GLASS, ORBIT REED, ORBIT CHOIR.
- PULSE: ORBIT SQUARE, ORBIT PWM, ORBIT PIN, ORBIT HOLLOW.
- FM4: ORBIT TINES, ORBIT METAL, ORBIT ROUND, ORBIT AURORA.

No OP-1 patch parameters or recordings were used to create these definitions. The names describe timbres, not acoustic instrument models.

## Integration and limits

- Legacy engine IDs 0–8 and their patch indices remain unchanged. Optional legacy SLICE retains ID 9 when enabled. New engines are appended after the legacy set (9–11 by default, 10–12 with SLICE).
- Existing saved sounds load their stored engines. New/empty projects use the new engines. Factory browsing starts with the twelve ORBIT sounds, followed by the existing bank. Browser order has changed; stored engine/patch IDs have not.
- New DSP source: `firmware/src/eng_orbit.c`. It does not call any legacy engine's render or note callback. Shared sine interpolation, amplitude ramps, fixed-point multiplication, voice scheduling, ADSR, mixer and FX remain part of the inherited framework.
- The new engines reuse the existing 84-byte `voice_t`. They add no heap allocation, persistent mutable global state, delay buffers or full framebuffer. Constants add code/flash space; target link size and stack headroom have not been measured.
- Tone is a stable one-pole filter, not the OP-1 filter. FM and PWM can alias at high notes and aggressive modulation; SWARM suppresses its second partial above Nyquist. No claim of identical OP-1 sound, matched loudness or target CPU performance is made.
- New patches are conservatively bounded in direct DSP tests; measured preset level trims have not been generated for them. Whole-mix gain/limiting still uses inherited code.
- Existing sample assets, drums, FX and envelopes remain SLOOP-derived. This release begins algorithm independence; it does not remove the framework or all upstream assets.

## Validation performed

`python tools/orbit_check.py`: 12 host test programs. New DSP tests cover all twelve patches, deterministic renders, notes MIDI 24–120, parameter minimum/maximum cases, output bounds, buffer guards and the 440 Hz carrier when FM depth is zero. The engine suite also passes with `FELUCCA_SLICE=1`, and under undefined-behaviour sanitisation. Full firmware C preprocessing and the HTTP/native PCM preview integration pass.

The web emulator runs these same C callbacks in a wasm32 module. The vendor pi32v2 target compiler/SDK and an FM-1 are unavailable in this environment; target compilation, linker RAM/flash accounting and IRQ timing remain required before hardware installation.


## FIRST LIGHT demo song

An original four-bar progression (Am7 / Fmaj7 / Cmaj7 / G7) at 108 BPM demonstrates FM4 ROUND bass, SWARM HAZE chord sustains, PULSE PIN plucks and the inherited synthesised 808 kit. Every track has 64 sixteenth-note steps. The composition is defined in `firmware/src/orbit_demo.c`, with no audio sample backing track.

Hold HOME, select DEMO SONG with PRESETS, press OCT+ twice, then PLAY. OCT− cancels the first confirmation. Playback must be stopped. Loading replaces the working project, while numbered saved project slots remain untouched. Normal autosave of the working project is still active, so save work before replacing it. This is a four-bar looping demo, not a section arrangement.

Native rendering: peak 0.723, RMS 0.183, zero clipped samples in the 12-second WAV. Listening copies are MP3 encodes of the actual C DSP. wasm32 tests exercise all twelve presets and the actual HOME menu/confirmation/PLAY path.
