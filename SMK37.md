# ORBIT on the M-Vave SMK-37

The M-Vave SMK-37 has the FM-1's chip (JieLi AC791N), so ORBIT runs on it as a target board: `BOARD=smk37`.
Everything SMK-37-specific sits behind `BOARD_SMK37`; the FM-1 build is unchanged, byte for byte. ORBIT's
workflow, screens and [user guide](GUIDE.md) apply as they are; the SMK-37's extra controls add to them.

> Unofficial firmware for the SMK-37, not affiliated with or endorsed by M-Vave. No warranty: install at your own
> risk and read [Safety](#safety) first.

## Models

| Model | Build | Package identity |
|---|---|---|
| SMK-37 Pro | `BOARD=smk37 sh build.sh` | `SMK-37 Pro_900` (`build/orbit-smk37.fwsc`) |
| SMK-37 Elite | `BOARD=smk37 MODEL=elite sh build.sh` | `SMK-37 Elite_900` (`build/orbit-smk37-elite.fwsc`) |
| MKE-P37 | `BOARD=smk37 MODEL=mke sh build.sh` | `MKE-P37_900` (`build/orbit-smk37-mke.fwsc`) |
| Starrykey 37 Play | the SMK-37 Pro build | `SMK-37 Pro_900` |

They are one keyboard under several names. The `MODEL` builds differ only in the name the keyboard reports (its
identity, its update loader's, the update record's), so M-Vave's updater still recognises the unit for the way back
to the stock firmware. Toolchain and SDK: [BUILDING.md](BUILDING.md). Both boards write the same files in `build/`:
copy each SMK-37 package out before the next build, and build the FM-1 last before `sh tests/run_tests.sh`.

## Install

Connect the keyboard with a USB data cable and use this repository's installer, as for the FM-1:

```sh
python3 tools/fm1_install.py build/orbit-smk37.fwsc      # Python 3 with mido and python-rtmidi
```

It finds the keyboard's MIDI port, checks the model and the package, asks the running firmware (the stock one or
ORBIT) to start the update loader and writes the package; then the keyboard restarts and the installed identity is
checked. The web installer (`web/installer.html`, built by `tools/make_site.py`) does the same in Chrome or Edge
when the site is generated with the SMK-37 packages (`--package` once per package): it writes the package for the
connected model. A package never goes to the other device (FM-1 / SMK-37), and a keyboard running a firmware with
its own updater is left alone.

Until this project's releases include SMK-37 packages, unofficial builds can be installed with the web installer:
<https://genkigenki.github.io/smk37/>.

**First start.** ORBIT keeps its projects, presets and settings in the keyboard's own storage area, tagged as
ORBIT's. The first start clears what another firmware left there (the stock settings, another port's projects).
The factory calibration, Bluetooth pairing and the stock resources are outside that area and never touched.
USR1 and USR2 hold user samples; USR3 and USR4 have no flash on the SMK-37 and stay empty.

**Back to the stock firmware:** install the official package for your model with M-Vave's own updater
(M-UPGRADE), or give it to the installer: `python3 tools/fm1_install.py --force SMK-37_Pro_016.fwsc`. ORBIT keeps
the stock update protocol, so the stock tools see the keyboard as usual.

**If it does not start:** after two failed starts ORBIT opens its USB recovery screen (identity `SMK-37 Pro_000`,
or the model's), where the installer can write a package again; after a third it drops into the chip's mask-ROM
update mode (UBOOT).

## Knobs, faders, wheels

| FM-1 printed | ORBIT function | SMK-37 printed | on the SMK-37 |
|---|---|---|---|
| MASTER (pot) | output volume | K1 | master volume (a virtual pot, 24 steps a click) |
| SELECT | tempo on Tape and in layers, pages elsewhere, menu section | K2 | SELECT |
| ALGORITHM | track 1–4; held step: lock parameter | K3 | ALGORITHM |
| PRESETS | sound on Tape / Mixer (with ORBIT's preset list), kit on drums; held step: its lock | K4 | PRESETS |
| KNOB 1 | the first value (blue) | K5 | KNOB 1 |
| KNOB 2 | the second value (green) | K6 | KNOB 2 |
| KNOB 3 | the third value (white) | K7 | KNOB 3 |
| KNOB 4 | the fourth value (orange) | K8 | KNOB 4 |
| — | — | FADER 1–4 | tracks 1–4 (4 = drums), by FADER BANK: A level, B pan (the Mixer's two pages), C reverb send, D delay send. A fader takes over once it moves, also after a bank change. |
| — | — | PITCH wheel | pitch bend of the selected synth track, ±7 semitones |
| — | — | MOD wheel | vibrato on the selected synth track, 10 Hz, up to ±1 semitone (up = more) |
| — | — | PEDAL jack | not used |

## Buttons

| FM-1 printed | ORBIT tap | ORBIT hold | SMK-37 printed | on the SMK-37 |
|---|---|---|---|---|
| PLAY | start / stop | — | SEQ PLAY | PLAY |
| REC | arm, record, close a free take, stop overdub | clear the selected track | SEQ REC | REC |
| HOME | Event Tape | settings menu | PATCH | HOME; also closes the FADER BANK screen and the pad sound picker |
| ENV | envelope pages | — | CHORD | ENV |
| LFO | modulation pages | — | NOTE REPEAT | LFO |
| FX | effect pages | punch-in effects | FX | FX |
| SEL | root, scale and chord pages | key and chord layer | SCALE | SEL; held, the pads are a piano (scale green, root brighter) |
| EDIT | engine pages | erase layer; EDIT + OCT− / OCT+: undo / redo | PARA | EDIT |
| ARP | arpeggiator pages | roll | ARP | ARP; lit while the selected track's arpeggiator runs |
| SEQ | step pages | step layer | SEQ | SEQ |
| GLO | Mixer, global pages | mix layer | GLOBE | GLO |
| SAVE | on Tape: SONG; on editing pages: SAVE pages | section and song layer | BT | SAVE; held, the pads show the sections (used green, free white) |
| OCT− | on Tape: capture; elsewhere: octave down | — | OCTAVE− | OCT− |
| OCT+ | on Tape: drop; elsewhere: octave up | — | OCTAVE+ | OCT+ |
| — | — | — | KNOB BANK | opens and closes the pad sound picker (lit while open) |
| — | — | — | FADER BANK | the FADER BANK screen; pressed again, `<` `>` or K2: the next bank A → B → C → D (closes after 4 s; lit while up) |
| — | — | — | `<` | page back (as SELECT, never the tempo); SEQ held: step page back |
| — | — | — | `>` | page forward; SEQ held: step page forward |

One screen of these at a time: FADER BANK closes the pad sound picker, KNOB BANK the FADER BANK screen.

## Keys and pads

| FM-1 | ORBIT | SMK-37 | on the SMK-37 |
|---|---|---|---|
| 27 keys F3–G5 | notes; in layers: tiles (steps, effects, mix, sections) | keys F3–G5 (the middle 27) | the same, with velocity |
| — | — | keys C3–E3 (the lowest 5) | synth tracks: notes; drum track: C3 kick, D3 snare, E3 hat (a black key plays its left white neighbour) |
| — | — | keys G#5–C6 (the highest 5) | synth tracks: notes; drum track: G#5 cowbell, A5 open hat, B5 crash, C6 ride |
| LEDs under the 27 keys | what is on or playing | 16 RGB pads | the 16 white keys F3–G5 with velocity: as steps and tiles top row 1–8, bottom row 9–16; as notes the low ones at the bottom. Lit in the screen's track colours (ORBIT's style: blue, green, white, orange): steps lit, playhead and hits bright, off dark. |

## The pad sound picker

KNOB BANK shows the sounds of one kind on the 16 pads and on the screen (8 × 2): basses, keys, organs, pads, leads,
plucks, stabs, effects, then the user presets, in PRESETS order, 16 a page; on the drum track the kits. It opens at
the selected sound's page. A pad loads its sound and plays a short preview; `<` `>` step the pages; KNOB BANK or
PATCH closes it. Turning K4 keeps ORBIT's own preset list on the screen.

## Power-on and special combos (as on the FM-1)

| Combo | Does |
|---|---|
| OCTAVE− held at power-on | USB rescue |
| OCTAVE− + OCTAVE+ held at power-on | hardware calibration (re-teach buttons and knobs) |
| OCTAVE− + OCTAVE+ held 5 s, stopped | update mode (UBOOT) |

## Not available on the SMK-37

- TRS MIDI IN (and MIDI clock over TRS): the FM-1's MIDI IN pin is a key-matrix row on the SMK-37. USB MIDI works
  as on the FM-1.
- User sample slots USR3 and USR4 (no flash behind them).

## Safety

- **Back up the whole 1 MiB flash first**, over the chip's mask-ROM UBOOT mode. The stock firmware has no way into
  UBOOT, so you need a USB_KEY dongle (as for the FM-1); with ORBIT installed, hold OCTAVE− and OCTAVE+ for
  5 seconds while stopped. Keep the dump private: it holds data unique to your unit.
- **Never write 0xC0000–0xC1FFF** (Bluetooth pairing and the factory key and pad calibration) **or 0xFF000–0xFFFFF**,
  and leave the stock resources (0xF4000–0xFDFFF) alone: they are unique to each unit and cannot be recreated. The
  firmware and its update loader write only an explicit allow-list of flash ranges (checked at compile time and
  again in the flash driver), but a manual UBOOT write can; restore only the ranges you need.
- `FELUCCA_UART` must stay 0 on the SMK-37: the build refuses otherwise.
- Power the keyboard from a reliable USB port during an update and do not unplug it while the update screen is up.
