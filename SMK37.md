# SLOOP on the M-Vave SMK-37 Pro: controls

`BOARD=smk37 ./build.sh` builds SLOOP for the SMK-37 Pro (same AC791N chip as the FM-1). How each FM-1 control and
its SLOOP function lands on the SMK:

## Knobs, faders, wheels

| FM-1 printed | SLOOP function | SMK-37 Pro printed | on the SMK |
|---|---|---|---|
| MASTER (pot, top left) | output volume | K1 | master volume (virtual pot, 24 steps a click) |
| SELECT (big encoder) | tempo (TRACKS), page ◀▶, visualiser style, menu section | K2 | SELECT: tempo / pages |
| ALGORITHM | select track 1–4 | K3 | track |
| PRESETS | sound (synth) / kit (drums) | K4 | sound / kit, the pad picker shows them |
| KNOB 1 | dial 1 on screen (blue) | K5 | dial 1 |
| KNOB 2 | dial 2 on screen (green) | K6 | dial 2 |
| KNOB 3 | dial 3 on screen (yellow) | K7 | dial 3 |
| KNOB 4 | dial 4 on screen (orange) | K8 | dial 4 |
| — | — | FADER 1–4 | tracks 1–4 (4 = drums), by FADER BANK: A level, B filter (low-pass ◀ open ▶ high-pass), C reverb, D delay; a fader takes over once moved |
| — (no wheels used by SLOOP) | — | PITCH wheel | pitch bend ±7 semitones, selected synth track |
| — | — | MOD wheel | vibrato 10 Hz, up to ±1 semitone (up = more) |
| — | — | PEDAL jack | — |

## Buttons

| FM-1 printed | SLOOP tap | SLOOP hold (layer) | SMK-37 Pro printed | on the SMK |
|---|---|---|---|---|
| PLAY | start / stop (LED flashes on every beat) | — | SEQ PLAY | PLAY |
| REC | record / arm | 2 s: clear track | SEQ REC | REC |
| HOME | TRACKS screen, again: visualiser | menu | PATCH | HOME |
| ENV | envelope pages (amp/filter shape) | — | CHORD | ENV |
| LFO | LFO pages (modulation) | — | NOTE REPEAT | LFO |
| FX | effect pages | punch-in FX on pads | FX | FX |
| SEL | key / chord pages | key layer (pads = piano: scale green, root brighter) | SCALE | SEL |
| EDIT | sound pages, drum kit | erase layer | PARA | EDIT |
| ARP | arp pages (MODE on K1: OFF switches the arpeggiator off) | note repeat (rolls) | ARP | ARP; LED lit while the selected track's arpeggiator runs |
| SEQ | step / pattern pages | step layer (steps 1–16) | SEQ | SEQ |
| GLO | global / master / system | mix layer (mute, solo, fill, tap) | GLOBE | GLO |
| SAVE | song screen / save | song layer (slots A–D: used green, free white) | BT | SAVE (SLOOP has no Bluetooth) |
| OCT− | octave down | drums: ghost hits | OCTAVE− | OCT− |
| OCT+ | octave up | drums: hard hits | OCTAVE+ | OCT+ |
| — | — | — | KNOB BANK | pins the pad sound picker |
| — | — | — | FADER BANK | the fader bank screen on / off (K2 turns A→B→C→D; LED lit off bank A) |
| — | — | — | < | page back (as SELECT, never tempo); SEQ held: step page back; visualiser: style |
| — | — | — | > | page forward; SEQ held: step page forward; visualiser: style |

## Keys and pads

| FM-1 | SLOOP | SMK-37 Pro | on the SMK |
|---|---|---|---|
| 27 keys F3–G5 | notes; in layers: tiles (steps, FX, mix…) | keys F3–G5 (middle 27) | same, with velocity |
| — | — | keys C3–E3 (lowest 5) | synth tracks: notes; drum track: C3 kick, D3 snare, E3 hat (black keys = left white key) |
| — | — | keys G#5–C6 (highest 5) | synth tracks: notes; drum track: G#5 cowbell, A5 open hat, B5 crash, C6 ride |
| LEDs under 27 keys | what is on / playing | 16 RGB pads | pads = the 16 white keys (top row 1–8, bottom 9–16), velocity; LEDs in the screen's track colours (blue, green, yellow, orange): steps lit, playhead/hits bright, off = dark; SCALE layer: piano layout; SAVE layer: slots used green, free white |

## Power-on and special combos (same as SLOOP on FM-1)

| Combo | Does |
|---|---|
| OCTAVE− held at power-on | USB rescue |
| OCTAVE− + OCTAVE+ held at power-on | hardware calibration (re-teach buttons and knobs) |
| OCTAVE− + OCTAVE+ held 5 s, stopped | update mode (UBOOT) |
