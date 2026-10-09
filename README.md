# ORBIT

**An experimental groovebox firmware for the FM-1, built on SLOOP/Felucca.**

ORBIT combines the existing synth, sampler and sequencer with an event Tape workflow and original graphics inspired by the OP-1's four-colour controls. Current release version: **0.4.1**.

> 현재 상태: pi32v2 타깃 빌드와 `build/orbit.fwsc` 생성·설치 형식 검증을 완료했습니다. **2026-10-09 실제 FM-1에 ORBIT 0.4.1을 설치하고 재부팅, USB INFO/PING 응답, 화면 표시와 버튼·노브 조작 반응을 확인했습니다.** 실기기 오디오 타이밍·장시간 안정성·복구 경로는 아직 검증하지 않았습니다. 기존 PHASE CPU·ALNK0 ISR 예산 초과는 원인 수정으로 해결했으며 기준과 허용치를 유지했습니다. 아래 [실기기 설치 테스트](#실기기-설치-테스트-2026-10-09)와 [검증 기록](docs/VALIDATION.md)에 확인 범위와 한계를 구분합니다. Bluetooth 헤드폰 기능은 구현되지 않았습니다.

웹 화면은 영어로 시작하며 Sloop 로고 대신 ultrathin `Orbit` 워드마크를 표시합니다. 펌웨어도 1픽셀 스트로크의 `Orbit` 부트 화면을 먼저 표시하고, 최소 750 ms 뒤 일반 UI를 그립니다. 새 framebuffer나 PCM 버퍼는 추가하지 않았습니다.

## 0.4.1 릴리스

[GitHub 릴리스](https://github.com/dspaudio/orbit/releases/tag/v0.4.1)에서 설치 패키지 [`orbit.fwsc`](https://github.com/dspaudio/orbit/releases/download/v0.4.1/orbit.fwsc)를 받습니다. [웹 에뮬레이터](https://dspaudio.github.io/orbit-web-emu/)는 같은 C 펌웨어를 WebAssembly로 실행합니다. 변경 사항과 설치·검증 한계는 [릴리스 기록](docs/releases/0.4.1.md)에 있습니다.

오리지널 OP-1의 Synth / Drum / Event Tape / Mixer, T1–T4 sound 모듈과 blue / green / white / orange encoder 역할을 반영했습니다. 믹서는 LEVEL / PAN / 기존 TRACK 편집을 제공하며, PHASE와 ALNK0 예산 초과는 기존 기준을 유지한 채 해결했습니다. 저장 형식 FUN5와 editor protocol v9는 유지합니다.

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

ORBIT engine IDs 0…11 remain unchanged; FM6 is appended as **12** (13 with the optional SLICE engine). Existing ORBIT 0.3 FUN4 projects migrate to FUN5 while retaining engine IDs, notes and sound parameters. Tape COPY / LIFT / DROP also carries nudges, fill conditions and remapped parameter locks. A drop that would exceed the 24-lock limit fails without changing the destination. Synth Tape의 EDIT+OCT− Undo와 EDIT+OCT+ Redo는 note 이벤트, nudge, fill 조건과 parameter lock을 함께 복구합니다.

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
| Mixer | Implemented; host tested | Four-track LEVEL / PAN, selected-track editing, mute, solo and effects |
| Project storage | FUN5 retained | Reads older FUN1–FUN4 projects; Tape selection and clipboard are temporary |
| Native preview | Implemented; host tested | Browser controls backed by the firmware's actual C DSP, sequencer and UI |
| Divide-by-zero trap mitigation | Applied; host register test passed | Explicitly clears EMU_CON bit 2 during IRQ initialisation |
| Target firmware build | Build and package verified | pi32v2 compile, ELF memory and static ISR budgets checked |
| Real FM-1 validation | 설치·재부팅 테스트 완료 (2026-10-09) | ORBIT 0.4.1 설치, USB INFO/PING, 화면 및 조작 반응 확인; 오디오·IRQ 타이밍, 장시간 안정성, 복구와 sample upload는 미검증 |
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
- **0.4.1:** Original OP-1 control roles, Orbit branding, preview input fixes and PHASE/ISR budget fixes; install package and updated WebAssembly emulator.
- **Next:** Validate boot, controls, USB and audio timing on FM-1 hardware. Bluetooth support remains a separate development task.

## Try the native preview

Requirements: Python 3, a C compiler (`gcc` or `cc`) and Pillow. Node.js is used for browser script syntax checks.

```bash
python -m pip install -r requirements.txt
python tools/orbit_preview.py --port 8080
```

Open http://127.0.0.1:8080 and press the audio-start button. The preview includes a demo pattern; the firmware boot project does not.

The preview uses the firmware DSP but buffers approximately 250–550 ms of browser audio, so it cannot establish device performance or playing latency. Its project save/load doubles are nonpersistent. Uploading user samples into the preview process is not supported.

미리보기는 버튼 탭만 지원합니다. HOME 길게 누르기와 레이어 조합은 실기기에서 사용합니다. SCL과 SELECT를 포함한 컨트롤, 짧은 건반 입력, Visualizer의 좌우 tap을 실제 C 입력·출력 경로에 연결했습니다.

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

Tape에서 GLO를 누르면 실제 트랙 믹서가 열리고, 믹서에서 다시 GLO를 누르면 글로벌 설정으로 이동합니다. 드럼 트랙의 Tape에서 EDIT 또는 SEQ를 누르면 DRUMS 페이지가 열립니다.

화면과 웹의 주요 흐름은 **Synth / Drum / Event Tape / Mixer**입니다. 오리지널 OP-1의 T1 engine / T2 envelope / T3 effect / T4 LFO는 FM-1의 **EDIT / ENV / FX / LFO**에 대응하며, 네 encoder는 **blue / green / white / orange** 순서입니다. 반복 입력으로 여는 기존 세부 페이지와 hold layer는 유지합니다. 믹서의 SELECT는 네 트랙 LEVEL → 네 트랙 PAN → 기존 선택 트랙 편집(SWING / LEVEL / LEN / PAN)을 순회합니다.

네이티브 미리보기는 실제 C 상태로 모드 선택을 표시하고, 웹 에디터의 Event Tape는 기존 sequencer 이벤트 편집을 엽니다. Drum은 트랙 4의 실제 편집 경로입니다. Engine 선택과 전체 sound preset 선택은 구분하며 PCM Tape 녹음·오디오 overdub·album 기능을 추가하지 않습니다. 적용 기준은 [DESIGN.md](DESIGN.md)와 [오리지널 OP-1 공식 가이드](https://teenage.engineering/guides/op-1/original)입니다.

Synth clips can move between synth tracks. Drum clips can only be dropped onto the drum track. Synth LIFT participates in the existing EDIT+OCT− undo path. A lifted drum region can be restored by dropping the clipboard; copying another region replaces that clipboard. Visualizer에서도 OCT±는 옥타브를 조절하며 Tape나 클립보드를 수정하지 않습니다. Outside HOME, the existing OCT± controls remain available.

For sample import and CHOP editing, use `web/editor.html`. Device communication uses the original SLOOP Web MIDI workflow and browser requirements; see [the upstream README](README-SLOOP.md).

## Validation

```bash
python tools/orbit_check.py
python tools/orbit_preview.py --port 8080
# Stop the preview after its first native build, then run:
python tests/orbit_preview_test.py
```

Linux amd64에서 `tools/orbit_check.py`의 **19개 호스트 프로그램**과 Node 24 환경의 `sh tests/run_tests.sh`가 통과했습니다. macOS의 새 counted CPU·golden 실행도 117 renders unchanged, CPU 초과 0건으로 통과했습니다. ALNK0 정적 비용은 기존 기준 174에 대해 138입니다. 기준이나 허용치를 완화하지 않았습니다. 웹·설치 CLI·네이티브 HTTP 검사와 실제 브라우저 1440px / 390px 조작을 확인했습니다. 패키지 검사는 `orbit.fwsc`의 CRC, 로더 마커, Python/JavaScript logical image 일치를 포함합니다.

See [validation details](docs/VALIDATION.md) for evidence and limitations. Host results do not establish target boot safety, RAM/flash usage or real-time audio performance.

### 실기기 설치 테스트 (2026-10-09)

SLOOP 2.4.1이 정상 동작하던 실제 FM-1에 소스 커밋 `3074510`에서 빌드한 ORBIT 0.4.1 패키지 `build/orbit.fwsc`를 `python tools/fm1_install.py build/orbit.fwsc --port Felucca --yes`로 설치했습니다. 설치 전 패키지 CRC를 확인했으며, 쓰기 100%와 자동 재부팅 후 설치 도구가 종료 코드 0을 반환했습니다. 재부팅한 기기의 INFO 응답은 `FELUCCA ORBIT 0.4.1`, PING 응답은 `[0]`이었고, 사용자가 ORBIT 화면 표시와 버튼·노브 조작 반응을 확인했습니다. 이번 설치에서 먹통 증상은 관찰되지 않았습니다.

테스트한 패키지의 SHA-256은 `40f238815bcd0864fbfdfee53854c58f7641d0ef20e350855e7f5d73389ff39b`입니다. 이 결과는 해당 패키지와 기기의 설치·부팅 확인이며, 모든 FM-1의 동작을 보장하지 않습니다. 오디오 출력·실시간 오디오/IRQ 타이밍, 장시간 안정성, sample upload와 실패 시 복구는 이번 테스트로 검증하지 않았습니다. OCT−를 누른 채 전원을 켜는 복구 진입 시도에서는 일반 화면이 나왔으므로, 실기기 복구 성공으로 기록하지 않습니다.

### Boot exception mitigation

Following the upstream Felucca #61 / #111 mitigation, `fm1_irq_init` explicitly clears EMU_CON bit 2, including a stale setting from a previous boot. Other register bits, exception configuration, vectors and branch tracing are preserved.

The test runs the actual initialisation function against RAM-backed MMIO on Linux amd64 and checks cold/warm states and repeated initialisation. 타깃 컴파일은 완료했지만 이 호스트 테스트는 실제 기기 부트 안전성을 보장하지 않습니다. macOS의 MMIO 테스트 컴파일 한계와 실기기 미검증 범위는 검증 기록에 구분합니다.

## Build for FM-1

Follow [BUILDING.md](BUILDING.md) for the JieLi pi32v2 Linux toolchain and AC79 SDK requirements.

```bash
export JIELI_TOOLCHAIN=/path/to/jieli/toolchain
export AC79_SDK=/path/to/AC79_SDK
python tools/build.py
```

The installable output filename is `build/orbit.fwsc`. Generated packages are not committed to this repository.

## Origin and licensing

Based on [isod89/sloop-fm1](https://github.com/isod89/sloop-fm1), originally commit `d691ba7b2d922f1a1f41a3622cffe29ce41c5506` (v2.3), with the v2.4.1 changes integrated in ORBIT 0.4.0. This is an independent repository created from a pinned source snapshot, rather than a fork containing the full upstream Git history.

Software is **GPL-3.0-only**. Original copyright notices and licence files are retained. Fonts and sample assets have their own licensing terms; see [LICENSING.md](LICENSING.md) and the notices in the asset directories. The original project documentation is preserved in [README-SLOOP.md](README-SLOOP.md).
