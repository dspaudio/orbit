# Official cluster kernel integration

## Implementation contract

This connects the cluster oscillator from the official original OP-1 #246 to ORBIT through a C kernel recovered from the real executable code. It isn't a renaming of the independently written SWARM/PULSE/FM4 as the official algorithm. CLUSTER is added after the existing engines, with default ID 15, or 16 in the optional SLICE build. Existing engine, factory preset and user sample IDs and the FUN5 layout are kept.

The original kernel works in 128-sample units, while ORBIT's control block is 32 samples. A per-voice 128-sample cache keeps the original smoothing, PRNG and control update period. Knob and pitch changes take effect at the next 128-sample boundary, and the ORBIT amplitude ramp is applied to each 32-sample block. State and cache live in the existing per-part shared engine arena, with a static check that they're smaller than the largest PHYS member. The waveform, normalization and curve tables are const data in flash.

The original names and raw signed16 knobs of the 16 factory clusters are preserved. Instead of widening the existing int8 factory format, this engine's factory load reads a separate raw table. Projects and user presets are stored in the existing int16 value array. Descriptors limit COUNT and CURVE to the safe range of the original lookups.

## Boundary between the original and ORBIT

The original kernel's fractional rounding, saturation, phase wrap, state and R0 return bit pattern are preserved. The original's full shared pitch/ADSR/FX wasn't recovered. In the product wiring, pitch, amplitude envelope, velocity, release length, mixer and FX are ORBIT, and the shared envelope of the factory patches is an explicit ORBIT default. So this doesn't claim full official patch sound or hardware equivalence.

The PRNG starts at seed 1 whenever an ORBIT part takes over the arena, and the voices within that part share it. This doesn't reproduce the call order of other engines and ADSR across the whole original system. The LFO pitch/amplitude paths are ORBIT, and FLT/SHP aren't mapped arbitrarily onto the original's internal parameters.

## Freeing sample budget

The original FLUTE and SCRCH recordings are replaced with small synthesized sounds.
FLUTE is a periodic loop of a fundamental with weak 2nd and 3rd harmonics.
SCRCH is a short one-shot mixing fixed-seed noise with a back-and-forth frequency sweep.
The waveforms are synthesized at build time and stored as IMA ADPCM, and the runtime uses the existing SAMPLE/GRAIN.
In other words, they weren't moved to a new real-time synth engine; they were replaced with different source sounds.
`LOFI FLUTE`, `SCRATCH`, `FLUTE DUST` and existing projects that select these banks make sound again.
The original set/zone/preset numbers, roots/key zones and USR1–4 flash addresses are unchanged.
The remaining sample data, drums and user samples are kept.

The original 54,284 B of data becomes 9,244 B of replacement sound, a net recovery of 45,040 B.
The three FLUTE zones use 6,172 B and the three SCRCH zones 3,072 B.
Each FLUTE zone provides at least 4,096 source samples, so GRAIN has material to work with.
Integer periods with matching source rates keep the pitch of the existing roots,
and the existing generator records the ADPCM loop-start state.
This change recovers flash. It doesn't mean lower general RAM/pool use or real CPU time savings.

## Verification status

On 2026-10-10 the product core and const table version were reconnected to the existing strict oracle,
and 729 cases, 5,103 blocks/R0, 653,184 samples and all state were compared byte for byte.
Everything matched, and the UBSan and generation reproducibility checks also exit 0.
The adapter's 16 raw patches, 128/32 cadence, PCM/PRNG state, release, arena,
multiple parts and WIDE/legacy transfer were confirmed with real C tests.

The strict macOS regression has 0 changed/gone, health, voice/routing,
CPU overrun and crash results across 210 renders. Only the three renders that reference the two replacement banks
were updated to the hashes of the real new output; the other existing hashes and CPU tolerances are kept.
Web signed16, user preset, library, lock and WATCH round trips and the native HTTP preview regression also passed.

With the replacement sounds, the full pi32v2 image is 548,960/581,564 B, RAM 85,908/98,304 B
and pool 333,948/344,064 B. All 33 static cost checks, including the new core/adapter,
passed. Static loop costs and host CPU figures don't prove real-device ISR time.
Real FM-1 audio timing, including the burst load of filling the 128-sample cache, is unconfirmed.
On 2026-10-10 ORBIT 0.6.0 was installed on a real FM-1, confirming the automatic reboot, INFO/PING,
the 16 factory raw values and preservation of 14 backup objects.
While MIDI was sent to the 16 CLUSTER presets and the three replacement sounds, a short real 48 kHz stereo
USB output recording confirmed non-silent output with 0 clipped samples.
This isn't a listening test, a per-sound quality judgment, a check for dropouts under worst-case load
or a long-term stability check. Full evidence is in the first entry of `docs/VALIDATION.md`.

The protocol adds v11/command 77 and keeps the existing v14 commands.
Preserving original values requires the signed16 envelope.
Rights to the extracted data and the reproduction steps are in `assets/op1-cluster/README.md`,
and full run results and limits are recorded in `docs/VALIDATION.md`.
