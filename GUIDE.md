# ORBIT 0.8.0 User Guide

ORBIT turns the M-VAVE FM-1 into three synth parts and a drum machine. This guide describes ORBIT 0.8.0, not the upstream SLOOP engine registry or installer. [Install/build](BUILDING.md) · [MIDI controllers](docs/MIDI-CONTROL.md) · [0.8.0 release notes](docs/releases/0.8.0.md)

SMK-37 source support is merged. The controls below use the FM-1 panel labels; see [SMK37.md](SMK37.md) for the SMK-37 Pro, SMK-37 Elite and MKE-P37 mappings, faders, wheels and pad sound picker. SMK-37 has only USR1/USR2 sample storage and no TRS MIDI IN. **Official v0.8.0 binaries are FM-1 only; SMK-37 binaries are not included.**

## Get started

1. Turn **ALGORITHM** to **track 4** (orange). The 16 white keys play drums; turn **PRESETS** for a kit, such as 808 or BOOMBAP.
2. On an empty project, press **REC** to arm. In **free** mode, play without a click and press REC on the downbeat after the last bar. ORBIT closes the loop, sets the tempo and starts playback. For a fixed tempo, select **tempo** on the REC screen first.
3. Press REC while playing to overdub. Hold **ARP** and a hat key for a roll. Hold **OCT- / OCT+** with a drum hit for ghost / hard levels.
4. Select track 1, choose a sound and record a bass line. Tracks 2 and 3 can hold different engines and sounds at the same time.
5. Hold **SEL** and press a key to set the song root. SEL + KNOB 1 selects one-key chords; SEL + KNOB 2 selects a scale. The physical SEL button is between FX and ENV; **SELECT** is a separate encoder.
6. Press HOME for Event Tape; use the [Tape controls](#event-tape) to move a phrase. Press GLO for Mixer.
7. Stop, save a section with **SAVE + white key 5-8**, and export a backup before an update.

## Panel and navigation

Tracks 1/2/3 use blue/green/white; track 4 and the fourth encoder use orange. Optional palettes retain text and selection markers. Red indicates recording.

| Control | Tap or turn | Hold / combination |
| --- | --- | --- |
| ALGORITHM | Select track 1-4 | While holding a step: choose lock parameter |
| PRESETS | Sound on Tape/Mixer; kit on drums | While holding a step: edit its lock |
| SELECT | Tempo on Tape/in a layer; pages elsewhere | Menu section in the menu |
| KNOB 1-4 | The displayed four values | The held layer's controls |
| MASTER | Output volume | USB recording follows it in MASTER mode |
| PLAY | Start/stop | Works inside layers |
| REC | Arm, record, close a free take, stop overdub | Hold to clear selected track; release before confirmation completes to cancel |
| HOME | Return to Event Tape; on Tape it stays on Tape | Settings menu; tap while holding a layer to lock that layer |
| EDIT / ENV / FX / LFO | Engine / envelope / effects / modulation pages | EDIT erases; FX performs punch-in effects |
| SEL | Root/scale/chord pages | Key and chord layer |
| ARP / SEQ / GLO | Arp / step / Mixer and global pages | Roll / step / mix layers |
| SAVE | On Tape: SONG; on editing pages: SAVE pages | Section and song layer |
| OCT- / OCT+ | Synth octave, including Tape; both together reset to 0. Drum GRID: previous/next step page | On Tape: SEL + OCT- capture, SEL + OCT+ drop; EDIT + OCT- undo, EDIT + OCT+ redo |

Tap a page button again to cycle its pages; turn SELECT to move backward/forward within that family, stopping at its ends. Each family remembers its last page. To reach SAVE pages from Tape, open an editing page first, then tap SAVE. Drum GRID/KIT navigation is described below. A locked layer frees both hands; another page/layer button unlocks it. PLAY, REC and octave controls remain available where the layer supports them.

In **0.8.1**, hold a layer button for **0.35 s** to show its layer page; a quick tap does not flash that page. Using a layer control while holding the button, such as turning a knob, shows it immediately.

Turning PRESETS on a synth track's Tape or Mixer **loads immediately** and opens a short list with nearby sounds. Further turns audition and extend the list. A value edit, navigation or Tape capture/drop dismisses it. Sound-design pages do not silently load sounds from PRESETS. Held-step locks keep their separate PRESETS meaning.

## Event Tape

HOME remains the event editor. It is not the macro-value page. Tape uses no audio recording: its reels and four lanes represent sequencer notes and drum events.

| Tape control | Action |
| --- | --- |
| KNOB 1 | Destination head |
| KNOB 2 | Region start |
| KNOB 3 | Region end, included in the selection |
| KNOB 4 | COPY or LIFT |
| SEL + OCT- | Capture the selected region; LIFT also removes it |
| SEL + OCT+ | DROP at the head, overwriting destination events |
| ALGORITHM | Select source/destination track |
| SELECT | Tempo |
| HOME | Stays on Tape |

COPY/LIFT/DROP carry chords, ties, hit levels, ratchets, nudges, fill conditions and parameter locks. A drop clips at the destination pattern end. Synth clips can move between synth tracks; drum clips only go to drums. A drop exceeding the 24-lock capacity is refused without changing the destination.

The clipboard and selection are temporary. Note and drum DROP operations support EDIT + OCT- / OCT+ undo/redo; synth LIFT also supports it. To restore a lifted drum region, drop the clipboard before copying something else. A rejected DROP preserves the previous Undo. Plain OCT-/OCT+ change synth octave on Tape without altering events or the clipboard; on drums they retain ghost/hard hit levels. SEL + OCT only edits Tape, not other pages. Tape's idle hint shows both shortcuts and follows COPY/LIFT.

## Recording and steps

**Stopped:** REC arms. The REC screen offers MODE free/tempo (free on an empty project), LENGTH 1/2/4 bars and START note/count. With START count, PLAY begins a one-bar count-in; notes during it only sound. **Playing:** REC starts overdub immediately and REC again ends it.

Free recording makes a loop from your playing; REC on the next downbeat closes it. PLAY abandons the free take. Tempo recording uses SELECT/GLO's tempo. The click at **GLO > GLOBAL > CLICK** is independent of the drum kit and is not recorded. Swing displays **0-100**: 0 is straight; triplet divisions are not swung.

Every track has its own 1-64 step length and division, from 1/32 to two bars. A synth step holds up to four notes; drums hold any combination of 16 lanes with per-hit level and ratchet. Different lengths can make polymeters.

| Step operation | Control |
| --- | --- |
| Set/clear a step | Hold SEQ and tap a white step key |
| Select step page | SEQ + first four black keys, or SEQ + OCT-/OCT+ without a held step |
| Pattern settings | SEQ layer KNOB 1 sound/note, 2 division, 3 swing, 4 length |
| Edit a held synth step | KNOB 1 note, 2 velocity, 3 ratchet, 4 nudge |
| Edit a held drum step | KNOB 1 lane, 2 level, 3 ratchet, 4 nudge |
| Parameter lock | Touch the destination on its page, then SEQ + step + PRESETS; ALGORITHM changes destination |
| Fill condition | SEQ + step + OCT+: normal / fill only / no fill |
| Clear detail | SEQ + step + OCT- clears nudge, locks and condition |

Nudge is -32..31 in 1/64 of a step. There are **24 parameter locks per track**. Only supported track sound parameters are lockable; global/actions and several performance controls are excluded. A knob edit while a lock is active establishes the new base value.

EDIT-layer SHIFT moves notes/hits, nudges, fill conditions and parameter locks together. LENGTH x2 copies all of these into the new half, replacing that half's old data. If the resulting pattern would require more than 24 locks, the entire operation is rejected with **NO LOCK LEFT**, preserving the pattern and previous Undo. A failed automatic lock insertion also preserves the previous Undo.

While playback and **REC on the selected track** are active, changing a supported track sound parameter with a page knob (including a mapped PERFORM knob) writes its value to the currently playing step. This uses the same 24-lock table as manual step locks: another change to the same step/parameter replaces that entry; each distinct step/parameter pair uses one slot. At capacity, a new entry shows **NO LOCK LEFT** and leaves stored locks unchanged, though the live value still changes. Global controls, pattern settings, actions and macro assignments are not captured; stopped edits or REC on another track do not write locks. This is discrete step automation, not continuous full-resolution motion or audio Tape.

For keyboard step entry, tap SEQ to STEP: panel or MIDI notes write the cursor's step; release all notes to advance. STEP KNOB 3 edits **LEN** when the cursor holds a note, extending it in steps; on a non-note it edits TIME. Step **VEL** is edited with SEQ + held step + KNOB 2. SELECT also reaches PATTERN and SONG. Ties and slide remain available. Undo/redo restores the last supported sequence edit, including synth nudges, fills and locks.

On track 4, tap **SEQ or EDIT from Tape/Mixer** to open DRUMS. Tap SEQ/EDIT again to switch **GRID / KIT**, or turn SELECT left for GRID and right for KIT. In GRID with no layer held, the eleven black keys silently select voices, left to right: **KICK, SNARE, HAT (closed), OPEN HAT, CLAP, RIM, LOW TOM, HI TOM, CRASH, RIDE, COWBELL**. The 16 white keys toggle only the selected voice's steps on the displayed page; setting a hit auditions it. **OCT-/OCT+** move backward/forward through 16-step pages and wrap at the pattern length, preserving the local cursor where possible and clamping it on a short last page; keys beyond LENGTH do nothing. **KNOB 1** selects and auditions any of the 16 voices, including those without black-key shortcuts; **KNOB 2** moves and auditions the step cursor, **KNOB 3** sets/clears its hit and **KNOB 4** changes its level. In KIT, keys play drums live again. Holding SEQ retains its first-four-black-key page shortcuts; holding EDIT retains erase/live-mute controls.

GRID shows all sixteen instrument rows against sixteen time columns, following the xOx-style matrix overview. BD/SN/CH mark kick, snare and closed hat; the selected row is highlighted. The cursor marks the edited cell, the playhead marks the sounding column, and the footer gives the absolute step number. Hit brightness shows level and small notches show ratchets. A persistent record indicator replaces the play triangle while REC is active. KIT remains the separate live-pad view.

Arming REC from GRID switches the keys to live recording while the REC/free-take screen is shown; they do not edit GRID steps underneath that screen. In GRID, simultaneous black/white input selects the voice before entering steps, while repeated step presses remain separate toggles. White-key and KNOB 3 deletion both clear nudge, fill and locks when the last hit is removed.

Mixer and KIT knobs are live controls, not automatic sound-parameter recording; their recording header says **NO AUTO**. Global and PATTERN pages also show **NO AUTO** while recording. FX headers distinguish **TRACK** settings from **ALL** (shared delay/reverb) settings. Unsupported drum sound pages explain the route back through HOME to DRUMS editing.

## Sounds and voice limits

The default build has **17 engines and 189 factory sounds**. The first twelve sounds remain ORBIT's SWARM/PULSE/FM4 presets; CLOUD's eight sounds are appended. Occupied USER slots follow the factory bank. To change a loaded sound's engine, open EDIT, tap SAVE for PRESETS, then turn KNOB 2. This loads the new engine's first preset, including its envelope, effects, LFO and default macro assignments; the track's pattern and mix settings stay intact. Choosing a sound also loads its sound settings and resets macro assignments to that sound's defaults.

| Engine | Character |
| --- | --- |
| ANALOG | Dual oscillators, noise, resonant LP/BP/HP filter |
| DIGITAL | Four-operator FM with eight algorithms |
| PHASE | Phase distortion |
| LOFI | Chip and bit-reduced synthesis |
| SAMPLE | Sample sets, including USR1-USR4 |
| VOICE | Formant synthesis |
| TRIO | Three oscillators with intervals and filter |
| WHEEL | Tonewheel registrations, percussion and rotary movement |
| GRAIN | Grains from sample sets, including user samples |
| SWARM | Original six detuned harmonic oscillators |
| PULSE | Original dual pulse/PWM oscillator |
| FM4 | Original four sine operators and three routing choices |
| FM6 | Six-operator msfa FM, DX7 import and B1-B27 bank |
| PHYS | Physical models of strings, resonators and percussion |
| NOISE | Noise sources and resonant filtering |
| CLUSTER | Translated original OP-1 #246 oscillator, 16 raw presets |
| CLOUD | Original oscillator-source grains, eight new presets |

The three synth parts share **eight sounding voices globally**. CLUSTER caps POLY/UNISON at four voices per part, CLOUD at three and FM6 at six; PHYS limits vary by model. MONO and LEGATO intentionally play one voice. UNISON spreads voices across one note; it is not a chord mode. New notes can steal voices, and CPU overload can fade extra voices. These are allocation limits, not guaranteed worst-case FM-1 polyphony. [CLUSTER details](docs/OP1-CLUSTER.md).

In **0.8.1**, drums use a free voice first. When all **six drum voices** are busy, a repeated sound reuses its own voice; a new sound steals the oldest non-kick, or the oldest kick if all voices are kicks. Kick protection applies to notes 35/36. This applies to GM, sampled and SYN kits while retaining existing hat choke and declick tails.

CLOUD's pages are **GRAINS** (WAVE, SIZE, DENS, SPRD) and **SPECTRUM** (DTUN, CHRP, STCK, TONE). WAVE morphs oscillator shapes; SIZE/DENS change grain length/start rate; SPRD adds timing/phase variation; DTUN spreads pitch; signed CHRP sweeps each grain; STCK introduces fifths/octaves; TONE filters it. It does not require a sample slot. GRAIN instead selects sample SRC and POS.

FM6's operator envelopes belong to the patch. Store edited DX7/FM6 patches into the bank before saving a project: projects retain the PTCH selection, not a private copy of the bank patch. CLUSTER uses ORBIT pitch, amplitude envelope, velocity and FX; its raw presets are not full original OP-1 patch reproductions.

## Envelope and LFO

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| ENV | ATK | DEC | SUS | REL |
| ENV DEST | FLT depth | PIT depth | SHP depth | SYNC |
| LFO | RATE | WAVE | PHS | FADE |
| LFO 2 | SYNC | TRIG | POL | Unused |
| LFO DEST | PIT | FLT | SHP | AMP |

Wide numeric ENV/LFO controls move **one value index per slow detent**; fast turns accelerate (12 indices per detent for the 0-127 and -64..63 ranges), clamped at the range ends. Named choices still move one choice per detent. **ENV SYNC** changes attack/decay/release displays to note values; sustain stays a level. **LFO SYNC** overrides RATE with a tempo division. TRIG NOTE restarts a fresh phrase at PHS, while FREE continues; POL BI is bipolar, UNI one-sided.

A moving LFO with all destination depths at zero does not change the sound. For a first test, set LFO DEST AMP to about half, then adjust source RATE/WAVE. FLT/SHP address the engine's modulation paths, not an arbitrary matrix of every knob.

## User macros and saving sounds

Macros belong to a **synth sound**, not to HOME or the global MIDI-learn table. Drum tracks do not enter PERFORM/MACROS.

1. Select a synth track and tap **EDIT**. Use **SELECT** to reach **MACROS**, adjacent to PERFORM.
2. Turn KNOB 1-4 to assign each encoder's destination. **DEFAULT** selects that engine's built-in destination. Duplicate destinations are allowed. Globals, actions and unavailable/fixed descriptors are not assignable.
3. Turn SELECT back to **PERFORM** to edit the four mapped values. Labels and ranges follow the current descriptors, including signed16 CLUSTER values. An unavailable destination is blank and does not edit a value.
4. Stop playback. Tap **SAVE**, select **USER** with SELECT, and choose slot 1-32 with **KNOB 1**.
5. Turn **KNOB 4 SAVE** one positive detent: AGAIN: SAVE. Turn it positively again within the confirmation window (about 1.5 s) to store the sound and its four assignments.
6. To recall on another synth track, select it, return to USER, pick the slot and turn **KNOB 2 LOAD** twice. **KNOB 3 ERASE** uses the same two-turn confirmation and removes the stored slot.

Changing the slot cancels an armed action. Factory sound loads, engine changes and INIT restore engine-default maps. A USER sound loads its saved map, including custom RES or TYPE assignments. Loading a USER sound keeps the destination track's live sequence and mix context, including **FILT, RES and TYPE**; it is not a project load. The OPB3 bank stores all 73 parameter values and the four macro assignments for backup/export, but sound recall does not replace those live filter values. Use a project/section to recall the whole track state.

Saving a project/section captures the current sound values and assignments as a snapshot; recall does not look up the USER slot again. Editing or erasing that USER slot later therefore does not rewrite an already saved section. Working autosave and full backups retain maps too. PERFORM's resolved destination can be used for step locks or MIDI learn where that parameter supports them.

## Effects and texture

FX pages provide drive and chorus/delay/reverb sends, track FILTER, SLICER, INSERT/INSERT 2 and global delay/reverb/chorus settings. INSERT supports **SOFT, HARD, FOLD, FUZZ, CRUSH, PHASR, FLANG, CHOR** plus OFF. Type changes the three control labels; INSERT 2 sets MIX. Drum INSERT is available too.

The shared delay's **DLY 2** page sets its stereo **TYPE**: **MONO** (the original centred echo), **WIDE** (the right echo 13 ms after the left) or **PING** (echoes alternate left and right, each bounce lowered by FDBK). PING splits the delay line in two, so its longest time is 0.74 s; longer TIME settings are held at that length. Changing TYPE silences the delay for about 25 ms while the line is cleared. TYPE is saved with the project and its song sections; projects from earlier versions open as MONO.

### Track filter

Select a synth track or track 4, tap **FX**, then use **SELECT** to reach **FILTER**.

| Control | Setting | Range and default |
| --- | --- | --- |
| KNOB 1 | FILT | -64..63; 0 is OFF/bypass in both types |
| KNOB 2 | RES | 0..127, displayed as a percentage; default 40/127, about 31% |
| KNOB 3 | TYPE | LP/HP (default) or BP |
| KNOB 4 | Unused | |

With **LP/HP**, negative FILT values close a low-pass filter and positive values open a high-pass filter. **BP** uses the same signed range to sweep the band-pass centre from low to high. Zero still bypasses it, rather than selecting the midpoint frequency. RES controls resonance in both types; its default retains the previous fixed-resonance setting.

This filter applies to every track, including drums. It is separate from ANALOG's engine filter and from the master filter, whose controls are unchanged. FILT and RES support step locks; TYPE is a live setting, not a lock destination. On synth tracks, RES and TYPE can be assigned on MACROS and edited through PERFORM.

Project/section saves retain each track's filter settings. Factory and USER sound changes leave the destination track's FILT, RES and TYPE alone, even when a USER sound restores macros that point to them.

### Performance effects and texture

Hold **FX** for 27 punch-in effects. White keys retain loops, stutter, reverse, stop, half-speed, filter sweeps, phone, crush, alias, gate, echo and wobble. The eleven black keys, left to right, add **KILL LOW, KILL MID, KILL HIGH, REWIND, OCT UP, FLANGER, AUTO-PAN, FREEZE, LOOP 8T, MONO, FADE OUT**. FX + OCT- / OCT+ triggers **REV / ECHO THROW**. Delay TIME now includes **1/4D** alongside the existing dotted choices.

FX layer knobs set master filter, DUST, DUCK and selected-track filter. DUST adds deliberate degradation; it is distinct from an unintended crackle fault. The look-ahead limiter, filter-state seeding and sound-change fades are implemented, but physical crackle resolution is unverified.

**GLO > TEXTURE** sets SRC, LEVEL, TONE and MOVE. **TEXTURE 2** sets RATE (1/2/4/8/16 bars), WHEN (PLAY/ALWAYS) and DUCK. Sources are OFF, VINYL, TAPE, RAIN, WIND, RADIO, ROOM and USR1-USR4. The bed uses no synth track, and an empty user slot is silent. Texture settings travel with projects/sections. ALWAYS can sound while transport is stopped.

On drums, hold **EDIT + a black key**, then use lane keys for live mutes. These are temporary performance mutes, reset by project application/NEW, not erased pattern events.

## Mixer, sections and projects

On Tape, **GLO** opens Mixer. SELECT moves through four-track LEVEL, four-track PAN and the existing selected-track controls. Press GLO again for the global pages. Hold GLO: white keys 1-4 mute, 5-8 solo, 9 holds fill, 10 schedules next-bar fill, 16 taps tempo; KNOB 1-4 set track levels.

| Save page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| PRESETS | Sound list | Engine only | Unused | Unused |
| USER | Slot 1-32 | LOAD | ERASE | SAVE |
| PROJECT | Slot 1-4 | ERASE | LOAD | SAVE |
| TOOLS | CLRSQ | INIT sound | Unused | NEW project |

Actions require confirmation; flash writes require stopped playback. PROJECT ERASE removes the chosen saved section, not simply the current track. Autosave keeps the working project after stopped inactivity; it is not a substitute for exporting a backup before updates.

**Demo removal and existing autosave:** 0.8.0 removes the built-in FIRST LIGHT loader and DEMO SONG menu, but updating doesn't erase an earlier working autosave. Old demo patterns can therefore return from storage. To clear them, back up any work you want to keep, stop playback, open an editing page, then tap **SAVE** and use **SELECT** to reach **TOOLS**. Confirm **KNOB 4 NEW**; the screen shows **NEW PROJECT**. Leave playback stopped so autosave can retain the empty working project. Saved sections are separate.

Hold SAVE and press white keys **5-8** to store A-D. Overwriting an occupied section asks for another press. Keys **1-4** recall sections, on the next bar while playing. Tap several while SAVE remains held for a quick chain (up to eight). Key **13** switches loop/song; **14** records the section order; **16** opens SONG. The SONG screen edits section order and bar lengths; stop before changing its settings.

## Samples, editor and USB audio

Use the Web MIDI editor in Chrome/Edge with SysEx access, served locally as described in [BUILDING.md](BUILDING.md), or from the installer site. Connect to the FM-1 before expecting a hardware operation to succeed. **?mock=1** exercises a simulation, not a connected instrument.

- **Sound/Library:** engine and sound editing, macro assignment, 32 USER slots, sound files and a browser-local library. Export the library before moving browsers/origins.
- **Sequencer/Event Tape:** tiles or piano roll, MIDI file import/export, locks, nudge/fill and drum lanes. This remains event editing.
- **Song/Tracks:** arrangement and mix.
- **Samples/CHOP/Drum kit:** USR1-USR4 on FM-1, USR1/USR2 only on SMK-37, with up to 16 WAV zones per slot. Chop imported audio and fit selected material to slot capacity. On FM-1, USR3+4 can hold a combined drum kit. Upload replaces slot contents, so back them up first.
- **Drum synth:** edit and store SYN1-SYN4.
- **FM6:** edit operators, import/export DX7 SysEx, store selected cartridge voices in B1-B27.
- **Projects/Backup/Settings:** slots, whole-device export/restore and globals. The full backup includes MIDI-learn settings.

USB audio is the master output, 16-bit stereo at 44.1 or 48 kHz. Select the Felucca input on the computer. HOME menu > AUDIO > USB AUDIO = MASTER follows the volume knob; FULL uses fixed recording level while MASTER still controls local listening. Reconnect USB after an update if the audio input is absent. On affected macOS versions keep USB SERIAL off and restart.

The [native preview](README.md#try-the-native-preview) runs the real C DSP/UI, but its save/load is nonpersistent, sample upload is unsupported and browser buffering is not FM-1 latency. Its controls support taps, not hardware hold layers.

## Menu

Hold HOME for settings. SELECT chooses SCREEN / LIGHTS / AUDIO / SYSTEM. SCREEN provides STYLE and ZOOM; LIGHTS provides button/key/note illumination; AUDIO provides LOWCUT, USB AUDIO and USB SERIAL; SYSTEM includes calibration, MIDI LEARN CLEAR and ABOUT. PRESETS moves the cursor, OCT+ acts, OCT- closes. These device settings are separate from song sound snapshots.

The full-screen visualizer was removed to free static RAM for future features. HOME on Tape stays on Tape. Pages that draw the output oscilloscope still do.

## Backup, updates and downgrades

**BACKUP before an update**, including before returning to an older firmware. Stop, connect the editor, open Projects / Backup and save the full file off the device. Preserve the pre-update backup separately from new 0.8.0 exports. Restore replaces contents and needs stopped playback.

ORBIT 0.8.0 writes **ORP8 projects** and **OPB3 USER preset banks**:

- **Projects remain 3840 bytes.** ORP8 adds each track's RES and TYPE without increasing the saved project size. ORP7 imports keep their sound and sequence data, remap engine-control lock and macro IDs to the new numbering, and set RES to 40 and TYPE to LP/HP.
- **OPB3 stores all 73 parameter values and four macro assignments.** Frozen readers import OPB2 and UPB1 banks. Old values and OPB2 macro destinations are mapped by their stored parameter count; missing controls take defaults. UPB1 has no saved custom macro map, so it uses engine-default assignments. Loading a USER sound still preserves the destination track's live mix, as described under [User macros](#user-macros-and-saving-sounds).
- Legacy ORBIT FUN1-FUN5 projects remain readable. Existing ORBIT engine IDs/order are retained. **Raw SLOOP 2.6 FUN6 is rejected**, because its registry/layout is not ORBIT's. Matching a legacy magic does not establish foreign-data compatibility.

**Older ORBIT, including 0.7.0 and 0.7.1, cannot read ORP8 or OPB3.** A downgrade can see slots as invalid/empty, and autosave can overwrite new work. Restore the matching pre-update backup on the matching old firmware. Don't assume SYN-kit/MIDI-setting preservation by old firmware that cannot read the new settings.

The [Pages installer](https://dspaudio.github.io/orbit/) validates the latest published package before Connect and checks identity before Install. Check that it shows 0.8.0 before installing this release. For local installation, use [BUILDING.md](BUILDING.md); default build identity is FM-1_900. Keep USB/power connected through the write and reboot. A failed download is not an invitation to skip validation.

The no-demo **0.8 hardware candidate** was installed on an FM-1 on 2026-10-27: write 100%, reboot and returned identity succeeded. The device owner confirmed the normal screen, cleared the earlier autosave with NEW PROJECT and accepted the hardware for release. INFO/PING timed out, so the firmware version was not confirmed by a successful query. The version-only **0.8.0 rebuild has not been separately hardware-tested**. Earlier 0.7.1 software checks, 0.7.0 installation/version/PING checks and the 0.6.0 USB recording remain historical. See [VALIDATION](docs/VALIDATION.md) for the full record.

If a write is interrupted, reconnect and use the installer resume path only when it identifies ORBIT's loader. The implemented recovery entry is OCT- at power-on; successful installation doesn't establish power-loss recovery. An FM-1 that no longer boots may require [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter); SMK-37 owners should follow the [board-specific safety guidance](SMK37.md#safety).

## Troubleshooting

| Symptom | Check |
| --- | --- |
| No MIDI/editor connection | Chrome/Edge, SysEx permission, data cable, direct connection; close conflicting MIDI applications |
| No controller sound | MIDI channels and IN setting; DIN-to-TRS adapter wiring; USB needs a host/bridge |
| Relative encoder jumps | Configure absolute 0-127 CC, not relative deltas |
| Silent synth black keys | SEL chord/WHITE mode can use them as modifiers rather than notes |
| LFO moves but sound does not | Enable a LFO DEST depth; check engine modulation destination |
| Chord thins out | POLY mode, engine cap, global eight-voice allocation and CPU shedding; reduce held voices/heavy effects |
| Macro value appears blank | Destination unavailable for this engine; choose a valid MACROS assignment |
| New save fails on old firmware | Use its pre-update backup, not ORP8/OPB3 |
| USER sound keeps the current filter | Expected: sound recall preserves FILT/RES/TYPE; use a project/section to recall the whole track state |
| Old demo patterns return after updating | The working autosave survives updates; back up wanted work, then confirm TOOLS > NEW |
| Unwanted physical crackle | Report engine/preset/notes/FX and the firmware version |

Full controller routing and reserved CCs: [MIDI-CONTROL.md](docs/MIDI-CONTROL.md). Original SLOOP/Felucca authors, GPL notices and third-party rights remain: [README credits](README.md#origin-credits-and-licensing), [LICENSING.md](LICENSING.md), [CLUSTER assets](assets/op1-cluster/README.md).
