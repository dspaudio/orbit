# ORBIT 0.7.0 MIDI Control

Play three independent synth parts and drums, route an external sequencer,
or use a controller's knobs for sound design. USB MIDI and the TRS MIDI IN
jack are supported. Bluetooth MIDI exists only in the separate 0.9.B build
(`orbit-ble.fwsc`, untested on hardware; see the [guide](../GUIDE.md#bluetooth-midi-build-09b)).
Direct USB keyboard hosting is not supported.
[User guide](../GUIDE.md) · [Install/build](../BUILDING.md)

## Connect a controller

- **DIN output:** controller MIDI OUT to the FM-1's 3.5 mm TRS MIDI IN using
  a DIN-to-TRS MIDI adapter with the correct wiring. A stereo audio adapter
  is not a MIDI adapter. Check the controller/adapter documentation and the
  actual FM-1 setup; this release does not establish one Type A/B choice for
  every adapter or a tested MiniLab hardware combination.
- **USB:** connect the controller and FM-1 to a computer and route the
  controller's MIDI output to the FM-1 in a DAW or MIDI bridge. Both are USB
  devices; a cable directly between them supplies no USB host. A separate
  host box must support the required devices and routing; it is not an ORBIT
  USB-host feature.

USB and TRS input can be received together. Avoid sending the same notes down
both paths unintentionally.

## Channels and clock

**GLO > MIDI:** KNOB 1/2/3 set CH1/CH2/CH3; KNOB 4 sets the drum channel.
Defaults are **1, 2, 3 and 10**. Select OFF or 1-16. Assigned channels are
kept unique. OFF disables that track's assigned input; output uses the
firmware's fallback channel rather than becoming a new arbitrary channel.

**GLO > MIDI 2:**

| Knob | Setting | Meaning |
| --- | --- | --- |
| 1 | IN | NOTES for normal routing; CLOCK for clock/transport only; CH 1-16 accepts that channel on the selected track |
| 2 | OTHER | SEL sends unassigned note channels to the selected track; OFF ignores those notes |
| 3 | OUT | KEYS sends panel playing; SEQ also sends sequencer, arp and rolls |
| 4 | BEND | Pitch-bend range, 1-12 semitones |

The SYSTEM page also exposes OUT, SYNC and IN. SYNC = INT uses ORBIT's clock;
USB or TRS follows that input's MIDI clock and START/CONTINUE/STOP.
Received notes are not echoed back as new output notes. Changing routing
releases held input/output notes. Sustain CC 64 holds synth note-offs while
the pedal value is at least 64; releasing it ends the waiting notes.

Program Change selects a sound-list index on synth tracks, or a drum kit on
the drum track. Indexing is zero-based on the wire; a controller's displayed
program numbering may start at 1. It is not an engine-ID selector.
MIDI notes on the STEP screen can enter notes/chords at its cursor.

## Standard CC map

Use **absolute seven-bit CC values, 0-127**. ORBIT scales the value over the
parameter descriptor's range; it does not decode relative encoder deltas.

| CC | Synth destination | Drum destination |
| --- | --- | --- |
| 5 | GLIDE | Not mapped |
| 7 | LEVEL | Global drum LVL |
| 10 | PAN | PAN |
| 71 | Engine RES or Q, if present | Not mapped |
| 72 / 73 / 75 | Release / attack / decay | Not mapped |
| 74 | Track FILTER | Track FILTER |
| 91 | Reverb send | Global drum REV |
| 93 | Chorus send | Not mapped |
| 94 | Delay send | Global drum DLY |
| 64 | Sustain pedal | No synth sustain |

Without a learned override, these follow the track selected by the input
channel. CC 74's center is filter off, lower values low-pass and higher
values high-pass. Engine resonance is ignored if the engine has no RES/Q.
The mod wheel and expression are not automatically an arbitrary modulation
matrix. Reserved messages cannot be made learnable merely by naming them
on the controller.

## MIDI learn: sixteen persistent assignments

1. Select the destination track.
2. Hold **GLO** and press **F#3** (the first black key) to enable learn.
3. Open the destination's track page and turn its FM-1 knob. For a mapped
   synth value, use **EDIT / SELECT > PERFORM**, not HOME. For synth mix
   LEVEL/PAN, use the Mixer page. A global page is not a learnable track knob.
4. Send the desired absolute CC from the controller.
5. Repeat for other controls, then **GLO + F#3** again to finish. Stop and
   leave the device idle so pending settings can be saved.

A learned CC addresses **one parameter on one stored track**. It replaces the
standard CC map for that CC number, regardless of which track its arriving
channel would normally select. Learning the same CC again, or the same
track/parameter again, replaces the previous assignment. There are **16
slots**, not sixteen per track or per MIDI channel.

Assignments are device settings, separate from the four sound macros. They
survive factory/user sound changes and project/section recalls and are saved
with MIDI routing settings. Full editor backup includes the MIDI settings
object. **HOME menu > SYSTEM > MIDI LEARN CLEAR** clears all assignments.

IN filtering still applies: CLOCK accepts no CC edits; IN = CH n accepts only
that channel. With normal IN = NOTES, a learned CC can still act from an
otherwise unassigned channel even when OTHER = OFF; OTHER primarily controls
unassigned note routing. Switching a controller's channel therefore does not
create another learned bank. Use distinct CC numbers for distinct mappings.

**Not learnable:** CC **0, 1, 6, 11, 32, 38, 64, 96-101 and 120-127**.
These retain reserved handling or remain ignored where no behavior is
implemented: bank select, mod wheel, data entry, expression, sustain,
RPN/NRPN/data increment/decrement and channel-mode ranges are not spare
assignment slots. The UI reports CC NOT LEARNABLE instead of recording them.

## MiniLab 3 example setup

In Arturia's MIDI Control Center, configure a user program with **absolute**
CC encoders and distinct nonreserved CC numbers for the eight encoders and
four faders. For example, **20-31** provides twelve learnable numbers; the
specific destinations are chosen on ORBIT, not inferred from MiniLab labels.
Do not use CC 32 for a thirteenth learned control.

Start with one encoder assigned to an audible parameter such as PERFORM
TONE or an envelope control, verify its endpoints, then learn the others.
The twelve encoder/fader controls fit in sixteen slots, leaving four slots
for other distinct CC controls. Pads sending notes do not consume learned
slots; pads sending additional CCs do. Relative modes, unlimited maps and
independent channel profiles are not supported.

Use the DIN-to-TRS path with the correct adapter, or a computer USB bridge.
These instructions follow the implemented MIDI parser and persistence
contract; **a physical MiniLab 3/FM-1 combination has not been tested for
this release**, and no hardware guarantee is implied.

## Save, backup and troubleshoot

- A USER sound or project saves its sound macro map, not a private
  MIDI-learn table. Export full Backup to keep MIDI learn/routing.
- Back up before updates and downgrades. Older firmware may not read
  0.7.0 settings; restore its matching old backup rather than assuming new
  routing/learn settings or SYN kits will survive.
- If a knob jumps, check absolute mode, its CC number and learn state.
- If notes do not play, check IN, assigned channels, OTHER, mutes and
  the DIN adapter. USB device-to-device cabling is not routing.
- Seven-bit CC covers signed16 CLUSTER ranges but cannot select every
  individual signed16 value; use the panel/editor for finer editing.

Host tests cover MIDI learn cold reload, changes of sounds/projects/sections,
backup object restore and flash-failure rollback. They do not prove a physical
power cycle, controller encoder feel or hardware MIDI timing. See the
[validation record](VALIDATION.md) for final release evidence.
