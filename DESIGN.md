# ORBIT: Original OP-1 Controls and Screens

## 1. Character and Identity

The reference is the [official original OP-1 guide](https://teenage.engineering/guides/op-1/original), not the OP-1 field. Thin lines on black, meaningful mechanical diagrams and blue / green / white / orange encoders define the interface. Keep the Orbit identity and existing engines; do not copy the original logos or artwork.

The official Synth envelope diagram has a `140×70` viewBox. On the FM-1's `240×240` LCD, separate the horizontal graphic area from supporting control hints. Use the existing `240×124` strip canvas without adding a full framebuffer.

## 2. Colour

| Role | Existing token / value | Use |
|---|---|---|
| Screen | `C_BLACK`, `#000000` | Black diagram background |
| First encoder | `TE_COL[0]`, RGB(40,124,255) | Blue |
| Second encoder | `TE_COL[1]`, RGB(30,204,112) | Green |
| Third encoder | `TE_COL[2]`, RGB(255,255,255) | White, replacing the earlier ORBIT yellow |
| Fourth encoder | `TE_COL[3]`, RGB(255,98,26) | Orange |
| Inactive | `TE_DIM`, `TE_MID` | Dimmed existing colours |
| Supporting lines and text | `TE_G1`–`TE_G4`, `pal` | Existing neutral greys |
| Record / delete | `te_alert` | Existing warning indication |

Preserve user-selected PASTEL, NEON and monochrome accessibility palettes. The web interface follows the same encoder order and distinguishes track colours from encoder roles.

## 3. Typography

Firmware retains the generated `FONT_S` and `FONT_L` fonts. Distinguish titles, values and hints without unnecessary oversized numbers. The web editor keeps its embedded `SLOOP Terminus` at 16px / 32px, `Fukiai` icons and system sans Orbit wordmark. The native preview keeps system sans. No new remote fonts are needed.

## 4. Spacing and Layout

The LCD keeps four 60px columns, 1px lines and insets in multiples of 4px. Read the central graphic first, then the four encoders' actual roles below it. The web layout separates primary modes from the four modules and keeps primary controls free of horizontal scrolling at 390px.

## 5. Shared Primitives and Control Contracts

### Primary Modes

Distinguish Synth / Drum / Event Tape / Mixer. Follow real FM-1 input: Synth uses EDIT on the selected synth track, Drum uses the drum track and its existing pages, Tape uses HOME, and Mixer uses GLO. Preserve track selection, transport, SEQ, SCL, ARP, SAVE and hold layers.

### Four Sound Modules

The original T1 / T2 / T3 / T4 roles are engine / envelope / effect / LFO, corresponding to EDIT / ENV / FX / LFO on the FM-1. Show physical key names alongside those roles. Preserve access to multiple parameter pages. A repeated press that cycles pages must not be presented as an enable toggle.

### Four Encoders

Always use blue / green / white / orange order. The default Envelope page is A / D / S / R. Mixer level controls address tracks 1 / 2 / 3 / 4, using the real `G_DRLVL` for drums. When editing pan, display all four track pans. Diagrams must match actual parameter changes.

### Engine and Sound Selection

EDIT, then SAVE > PRESETS > KNOB 2 changes the engine by loading its first preset, including envelope, FX, LFO and macro defaults. Pattern and mix settings remain intact. Do not present unsupported Shift browsing or sound slots as active controls.

### Drum Matrix

The user's xOx reference (`x0x-4855ed23db355cb8.jpg`, supplied on 2026-10-14) defines the central display: instrument names down the left, sixteen time columns across, orange hits on black and a distinct selected instrument row. GRID is a matrix overview, not sixteen large pads for one instrument. KIT retains its live pads.

On the 240x240 LCD, GRID uses a 24px compact header, sixteen 10px instrument rows and the existing four encoder controls at y=184. The matrix starts at x=24, with 13px column pitch and 11x8px cells; four-step groups remain visible. Two-letter drum labels reuse the generated FONT_S bitmap compressed vertically to 8px; no new font asset or framebuffer is allocated. Draw the 160px matrix in two 80px canvas strips.

Show all sixteen ORBIT lanes, the selected row, current step cursor, transport playhead, hit levels and ratchets. The eleven black keys select voices; sixteen white keys toggle the displayed time columns. OCT changes the 16-step page. Existing orange drum tokens, neutral greys and blue/green/white/orange encoder roles remain authoritative. Keep short final pages visibly bounded and preserve the existing CPU and memory limits.

### Event Tape

Two reels and four track timelines represent actual sequencer events. Lift / Drop / Copy / Paste preserve events, nudges, fills and locks. Label it Event Tape; do not imply PCM recording, tape speed / pitch, audio overdubbing or album features.

### User Macros

EDIT / SELECT reaches PERFORM for four mapped sound values and MACROS for destination assignments. Both use the existing four-column layout and blue / green / white / orange encoder roles. PERFORM labels and ranges come from the selected sound's actual descriptors; an unavailable destination is blank and cannot change a value. MACROS distinguishes engine defaults from explicit assignments. User presets and projects retain the mappings with the sound.

HOME remains Event Tape. It does not add a third sound-control view or replace Tape's encoder actions.

### Web Control States

Mode and module selection call the existing editor or C host paths. Indicate selection through text, contrast and `aria-pressed`. Buttons need clear hover, pressed, focus and disabled states. Never report a successful hardware MIDI operation while disconnected. Retain the notice that preview project saving is a nonpersistent host double.

## 6. Motion and Feedback

Keep lazy LCD redraw and real transport / parameter animation. Web button feedback takes 100–150ms; existing panel transitions stay within 200ms. Do not add layout animation. Respect `prefers-reduced-motion`. Report actual asynchronous errors and never infer success from an arbitrary delay.

## 7. Depth and Surfaces

The LCD uses flat black and thin lines. Preserve the existing web device shell without adding decorative cards, gradients or shadows. Modes and modules are equally sized functional buttons. Do not add ornamental OP-1 imitation controls.

## 8. Accessibility and Verification

Keep keyboard access, visible focus, readable contrast and actual control labels on the web. Verify mobile and desktop rendering, buttons, encoders, transport and editing paths. Check firmware through real host C captures, pi32v2 builds, memory usage and existing budgets. These checks do not replace FM-1 hardware timing or flashing tests.

## 9. Preset Browser and Web Installer

The transient preset browser reuses the existing seven-row sound list and strip canvas. A PRESETS detent immediately auditions the selected sound wherever preset browsing is already available. Show adjacent sound names and distinguish the current row with text and contrast, not colour alone. Further turns extend its visibility; a parameter edit or navigation dismisses it. Preserve held-step parameter locks and existing restrictions on loading presets while editing.

The installer starts with a concise ORBIT introduction, musical features and actual native firmware screenshots, then the installation task. Use a readable centred column, a compact Orbit wordmark and the blue / green / white / orange identity. The document owns vertical scrolling; no fixed panels or nested scroll areas are needed. Download and validate the latest published release automatically, then offer Connect and Install. Do not expose a file picker. A failed download offers Retry download; it never enables MIDI or installation.

Use a plain three-item feature list and three native LCD screenshots: Envelope, preset browser and Event Tape. The captures must come from the real C renderer, not recreated artwork. Use three equal gallery columns with 16px gaps at desktop widths and one centred column below 640px; keep the screenshots' 240px aspect ratio, captions and readable pixel edges. Include meaningful alt text. Do not introduce decorative cards, a fake device shell or an image carousel.

Installer CSS tokens:

| Token | Dark | Light |
|---|---|---|
| `--background` | `#000000` | `#f7f7f7` |
| `--surface` | `#111111` | `#ffffff` |
| `--text` | `#ffffff` | `#111111` |
| `--muted` | `#aaaaaa` | `#555555` |
| `--line` | `#444444` | `#cccccc` |
| `--accent` | `#287cff` | `#185ad8` |
| `--success` | `#1ecc70` | `#127c43` |
| `--warning` | `#ff621a` | `#b83900` |

Use system sans for body text at 16px, supporting text at 14px, section titles at 24px and the ultrathin Orbit wordmark at 32px. Status and package digests may use system monospace. Use a 4px spacing unit, a 720px content limit, 24px desktop / 16px mobile page padding and controls at least 44px high. Long filenames, URLs and digests wrap without horizontal overflow.

Shared installer primitives are labelled native buttons, file inputs, a progress element and a live status region. Buttons cover idle, hover, focus-visible, pressed, disabled and working states. Keep separate Connect and Install actions: request SysEx permission only from Connect, and enable Install only after package validation and device preflight. During a write, disable conflicting actions and retain progress and recovery instructions. Declare success only after the expected device identity returns. Use visible errors, keyboard-operable controls and a reduced-motion path; no animated layout, decorative shadows or gradients.
