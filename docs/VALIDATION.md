# ORBIT 검증 기록

## ORBIT 0.4.1 릴리스 빌드 — 2026-10-09

기본 소스 버전 `ORBIT 0.4.1`로 다시 빌드했습니다. 설치 identity는 기존 `FM-1_900`을 유지하며 `--release X.Y`의 upstream 형식을 바꾸지 않았습니다.

| 검사 | 현재 릴리스 결과 |
|---|---|
| `PYTHON=~/.jieli/orbit-venv/bin/python sh build.sh` | exit 0. app 570,576 B, loader 6,863 B, `orbit.fwsc` 610,019 B |
| ELF 메모리·타깃 예산 | `.data + .bss` 83,972 / 98,304 B, pool 334,560 / 344,064 B. `.ram_text` 925 instructions, call 없음. ALNK0 cost 138 / baseline 174 |
| Linux amd64 / Node 24 | `python3 tools/orbit_check.py` 19/19 PASS, `sh tests/run_tests.sh` **ALL HOST TESTS PASSED**, exit 0. stress·sanitizer·soak 조건 유지 |
| 새 macOS `cc -O2` counted regress | 117 golden renders, changed/gone 0, health·voice·CPU 초과·crash 0, exit 0 |
| 최신 native 미리보기 | 먼저 C host 재빌드 후 HTTP/DOM·44.1kHz stereo PCM·실제 좌우 visualizer tap PASS, exit 0 |
| 설치·웹 | 전체 러너에서 실제 패키지 CRC·Python/JS logical image·loader marker 및 editor 검사 PASS |

`orbit.fwsc` SHA-256: `31afad04333e6593e0333039c53f26b53ccab032b665cdc8e4aa3ec82bb53253`.

로그는 `build/host/release-0.4.1-build.log`, `release-0.4.1-tests.log`, `release-0.4.1-counted-preview.log`입니다. Linux timed CPU의 21개 참고 메모는 실패 판정이 아니며, 엄격한 CPU 판정은 별도 macOS instruction counter 결과입니다. 기존 독자 엔진 12개 preset의 golden·CPU baseline 부재는 유지합니다.

웹 에뮬레이터의 소스 pin과 생성 결과는 별도 [저장소](https://github.com/dspaudio/orbit-web-emu)의 `ORBIT_REVISION`·`build-info.json` 및 검증 기록에서 확인합니다. FM-1 실기기 플래싱·부트·USB·오디오 타이밍과 Dots 배포는 수행하지 않았습니다.

## 오리지널 OP-1 조작·기존 예산 원인 수정 — 2026-10-09

기준은 OP-1 field가 아닌 [오리지널 OP-1 공식 가이드](https://teenage.engineering/guides/op-1/original)입니다. [DESIGN.md](../DESIGN.md)에 따라 Synth / Drum / Event Tape / Mixer, T1 engine / T2 envelope / T3 effect / T4 LFO, blue / green / white / orange 역할을 실제 C 화면·입력, native preview와 Web MIDI editor에 반영했습니다. 기존 세부 페이지·hold layer·선택 팔레트·이벤트 클립보드·저장 형식·장치 프로토콜·라이선스를 보존했습니다. Event Tape는 sequencer 이벤트 편집이며 PCM 녹음이 아닙니다.

### 원인 수정과 엄격한 예산

- `eng_phase.c`: resonance 창의 case를 분리하여 쓰지 않는 창 계산을 제거했습니다. 창 정수식 196,608개 비교에서 차이는 0이며, 기존 PHASE `03_RESO_PLUCK` golden `0f46c3e6fb6a0c15`를 유지합니다.
- `voice.c`: overload victim 검색을 평탄한 두 순회로 바꿔 최악 slot 방문을 72에서 48로 줄였습니다. release 우선, 동률 순서, POLY bass와 MONO/LEGATO/UNISON lead 보호, stage-4 fade를 기존 회귀로 확인했습니다. 작업을 다른 ISR이나 미계측 helper로 옮기지 않았습니다.
- `tests/golden.txt`, `cpu_baseline.txt`, `target_budget.txt`와 허용치를 변경하지 않았습니다. 이전 PHASE 1,154 / baseline 899(+28%) 실패는 최종 macOS instruction-counted 실행에서 해소됐습니다. stock formatter는 통과 항목의 개별 CPU 수치를 출력하지 않으므로 새 수치를 추측하지 않습니다. 전체 CPU 초과는 0건이며 +25% 기준을 그대로 적용했습니다.
- 최종 `fm1_alnk0_irq`: 221 instructions, loop 90, divide 0, loop call 2, **cost 138 / baseline 174**, 이전 cost 268 대비 감소했습니다. 전체 타깃 정적 예산 검사 exit 0입니다.

### 최종 실행 결과

| 검사 | 직접 확인한 결과 |
|---|---|
| pi32v2 빌드·패키지 | exit 0. app **570,576 B**, loader **6,863 B**, `build/orbit.fwsc` **610,019 B**, `FM-1_900` |
| 실제 ELF 메모리 | `.data + .bss` **83,972 / 98,304 B**, pool **334,560 / 344,064 B**, 여유 **9,504 B**. `.ram_text` **2,964 B**, 925 instructions, call 없음 |
| Linux amd64 `python3 tools/orbit_check.py` | **19/19 PASS**, IRQ RAM-backed MMIO 포함. `build/host/orbit-checks.txt` |
| Linux amd64 / Node 24 `sh tests/run_tests.sh` | **ALL HOST TESTS PASSED**, exit 0. 기본 stress 40,000 frames, ASan/UBSan 15,000 frames, 10분 가상 soak를 줄이지 않았습니다 |
| macOS fresh `cc -O2` counted regress | **117 golden renders, 0 changed/gone, 0 health/voice/CPU failures, 0 crashes**, exit 0 |
| 설치·웹 검사 | 실제 패키지 CRC·loader marker·Python/JS logical image, 기존 SysEx·샘플·backup/restore·OTA 검사 PASS |
| native 재빌드·HTTP/DOM 검사 | 실제 44.1kHz stereo C PCM, state metadata, 초기 Tape·rapid Drum→Synth, 짧은 note·blur·pointer cancel, 좌우 tap PASS. 서버 정리까지 exit 0 |
| 실제 화면·브라우저 | 실제 C 240×240 Tape·engine·ENV·FX·LFO·mixer 캡처 확인. Chrome 1440px / 390px DPR3 touch에서 실제 주요 모드·모듈 이동, hover 중 선택 표시, overflow 없음 확인. 실제 Web Audio에서 2채널 non-silent PCM 디코딩 |

메모리 값은 ELF의 `_bss_end=01c1c804`, `_pool_start=01c20000`, `_pool_end=01c71ae0`에서 직접 계산했습니다. full framebuffer나 PCM tape 버퍼를 추가하지 않았습니다. mixer는 SELECT로 LEVEL / PAN / 기존 TRACK(SWING / 선택 트랙 LEVEL / LEN / PAN)을 열며 drum level은 실제 `G_DRLVL`입니다.

### 환경 한계와 수행하지 않은 검증

macOS 전체 러너의 ASan은 sanitizer allocator 초기화 자체의 `CHECK failed`로 실패했습니다. 같은 기본 stress를 Linux ASan/UBSan에서 확인하고, 최종 전체 러너도 Linux에서 통과시켰습니다. Debian 기본 Node 18의 `deflate-raw` 미지원 실패는 Node 24로 환경만 바꿔 해결했습니다. Linux timed CPU는 비교 참고이며 엄격한 CPU 판정은 별도의 macOS instruction counter 결과입니다.

기존 SWARM/PULSE/FM4 12개 preset의 golden·host CPU baseline 부재는 그대로 보고됐으며 새 기준을 만들어 검사를 통과시키지 않았습니다. standalone C LSP는 생성 헤더·unity build 설정을 이해하지 못하고 JS/Python 언어 서버도 설치되어 있지 않아 실제 컴파일·런타임으로 확인했습니다.

FM-1 연결·플래싱·실기기 부트, USB 전송, Flash 전원 차단, ISR 사이클·오디오 underrun·지연, Dots 배포는 수행하지 않았습니다. preview의 project save는 비영구적 host double입니다. 아래 기록은 수정 전 또는 이전 버전의 당시 결과입니다.

## 기능 연결·설치 패키지·브랜딩 — 2026-10-09

환경: Apple M2 / macOS, Docker Linux amd64, `jieli-linux-toolchains-20250324.1`, AC79 SDK 태그 `AC79NN_SDK_V1.2.1_2023-12-13`. SDK의 `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` SHA-256이 빌드 스크립트의 기준과 일치했습니다. Python 환경은 `~/.jieli/orbit-venv`, 툴체인은 `~/.jieli/toolchain`, SDK는 `~/fw-AC79_AIoT_SDK`에 설치했습니다.

| 검사 | 결과 |
|---|---|
| `PYTHON=~/.jieli/orbit-venv/bin/python sh build.sh` | PASS. 실제 pi32v2 컴파일·링크 및 로더·패키지 생성 |
| 앱 / 패키지 | `build/felucca.bin` 569,600 B / `build/orbit.fwsc` 610,019 B, identity `FM-1_900` |
| 타깃 메모리 | `.data + .bss` 83,972 / 98,304 B, `.pool` 334,560 / 344,064 B. `.ram_text` 925 instructions, 내부 call 없음 |
| `python tools/orbit_check.py` | Linux amd64에서 19/19 PASS. 결과: `build/host/orbit-checks.txt` |
| `python tests/install_test.py` | PASS. 실제 `orbit.fwsc`의 CRC·로더 마커·Python/JS logical image 및 시뮬레이션 설치 경로 |
| `node web/test_web.mjs` | PASS. 복원 data 실패 뒤 abort, abort 실패 시 원래 오류 유지, 기존 프로토콜·샘플·FM6·프로젝트 검사 |
| `python tests/orbit_preview_test.py` | PASS. 실제 C 스테레오 PCM, HTTP 입력 거부, SCL, 짧은 건반 입력, 실제 render 명령의 좌우 tap 일치 |
| 브라우저 | 실제 WebKit에서 영어 에디터 8개 탭, 1440 px / 390 px 화면, `Orbit` 워드마크 weight 100, C 엔진의 무음이 아닌 2채널 디코딩·Visualizer 파형 확인 |
| 펌웨어 부트 렌더 | 실제 `splash.c`의 `Orbit` 1픽셀 워드마크 확인. `orbit_splash()` 후 최소 750 ms 동안 일반 `ui_draw()`를 지연하며 새 framebuffer·PCM 버퍼는 없음 |
| 빌드 옵션·파일명 | 실제 C 컴파일 인자로 UAC 0/1 전달 확인. 기본·릴리스 CLI 모두 `orbit.fwsc`에 쓰는 경계 검사 PASS |

패널 진입·은행 staging 만료·좌우 tap의 새 회귀는 수정 전 실패를 직접 재현한 뒤 같은 검사로 통과했습니다. Tape의 GLO→믹서→글로벌 설정, 드럼 EDIT/SEQ 진입, 15초 staging 만료와 타이머 wrap 후 실제 NOR 쓰기를 확인했습니다. 웹 복원 중단 뒤 abort와 원래 오류 유지도 현재 소스의 테스트에서 통과했습니다. 생성된 부트 캡처는 `build/host/boot-qa/orbit-boot.png`에 있습니다.

### 수정 전 기존 예산 실패

`sh tests/run_tests.sh`는 exit 1입니다. 전체 로그 `build/host/full-tests.txt`에서 아래 두 예산 초과와 최종 실패 요약만 확인됐으며, 나머지 저장·복구·시퀀서·UI·DSP·USB descriptor·로더·설치·웹 검사는 통과했습니다.

- 호스트 `cpu/PHASE/03_RESO_PLUCK`: 1,154 instructions, 기준 899, +28%, 허용 +25%. 수정 전에도 1,153으로 이미 초과했습니다.
- 타깃 `fm1_alnk0_irq`: 정적 비용 268, 기준 174, +54%, 허용 +10%. 수정 전 타깃 빌드에서도 동일하게 초과했습니다.

기존 기준과 허용치를 변경하지 않았습니다. 처음 타깃에서 측정한 SWARM·PULSE·FM4·FM6 여섯 함수의 기준만 추가해 더 이상 `no budget`으로 지나가지 않게 했습니다. 정적 instruction 비용과 호스트 CPU 값은 실제 FM-1의 사이클·오디오 underrun 측정이 아닙니다.

macOS의 `irq_init_test`는 Mach-O section 지정과 Linux `MAP_FIXED_NOREPLACE` 때문에 직접 컴파일되지 않습니다. Linux arm64 실행도 성공하지 않았으므로 해당 결과를 검증 근거로 쓰지 않습니다. 같은 실제 MMIO 하네스와 필수 검사 전체는 Linux amd64에서 통과했습니다. standalone C LSP는 unity build의 include·pi32v2 설정을 이해하지 못하고, Python/JS 언어 서버도 설치되어 있지 않아 실제 컴파일·런타임 검사로 검증했습니다.

FM-1 MIDI 입력·출력은 연결되지 않았습니다. 실제 기기 설치, 부트 화면 전환, USB enumeration/전송, Flash 전원 차단, IRQ 타이밍, 오디오 부하·지연, Dots 배포는 수행하지 않았습니다. 설치 패키지 생성·형식·메모리 검증 통과를 실기기 안정성 확인으로 해석하지 않습니다. 생성 패키지는 저장소에 커밋하지 않습니다.

아래는 이전 버전의 당시 검증 기록입니다.

## ORBIT 0.1 validation — 2026-10-07

Host: Linux x86_64, gcc, Python 3.12. No FM-1 hardware connected.

```text
orbit_test: PASS
seq2_test: PASS
project_test: PASS
storage_test: PASS
drumkit_test: PASS
studio_drums_test: PASS
punch_test: PASS
song_audio_test: PASS
song_ui_test: PASS
```

The ORBIT test exercises event clipboard copy/lift/drop, chord/tie/dynamics preservation, destination bounds, synth/drum type rejection, real panel inputs, transport, and 2,000 UI frames with track/length changes. Framebuffer bounds are asserted.

HTTP preview integration: PASS. The native firmware generated non-silent 44.1 kHz stereo PCM through the HTTP bridge. Panel operations and invalid input rejection were checked. Browser JavaScript passed node --check. A full browser audio session was not exercised.

The full firmware translation unit passed preprocessing with its HAL and generated headers. Target package build was attempted and stopped because the AC79 SDK files were missing. The pi32v2 toolchain is also absent; its vendor endpoint timed out. No installable .fwsc is included. Target binary size, BSS, IRQ timing, USB hardware and Bluetooth were not tested.

Existing web editor suite: PASS (sample byte parity, import/CHOP, backup/restore, protocol, mock upload and OTA error paths). Package tests were skipped because no target .fwsc exists. Native host compilation produces existing HAL pointer-width warnings on x86_64; no warning originates in the ORBIT additions.

## 0.2 graphics update

All nine host test programs passed after the UI changes. Tape, all engine pages, envelope, LFO, mixer and sample output rendering were exercised. The host preview now writes its actual mixed PCM into the scope ring, matching the hardware scope source. The native HTTP preview passed again, and firmware preprocessing passed. Target toolchain/SDK and hardware validation remain unavailable.

## 0.2.1 IRQ initialisation mitigation

The real fm1_irq_init now explicitly clears EMU_CON bit 2 instead of setting it. The Linux RAM-backed MMIO test passes with zero, bit-2-set, all-bits-set and mixed initial states; unrelated EMU bits, branch trace, exception configuration and vectors are preserved, and a second initialisation remains safe. This is a host register-write test, not a target compiler or FM-1 hardware test. The existing recovery test and all nine functional test programs passed. Firmware preprocessing passed. Target compilation and hardware validation remain pending.

Sequence screenshots are rendered by the actual STEP and PATTERN UI with a representative 16-step pattern and a chord. The screenshot fixture does not change firmware boot defaults.

## HOME preset feedback correction

All 68 factory browser entries resolve to a real engine preset. Native panel tests verify that consecutive and multiple PRESETS detents load individual bass sounds and that HOME feedback matches the actual selected engine/preset. Tape layout is unchanged.


## ORBIT 0.3.0 independent synthesis and FIRST LIGHT

- All 12 host programs pass, including the new independent DSP suite.
- All twelve new patches: deterministic direct renders, MIDI 24–120, parameter min/max, output bounds and guards. FM4 depth-zero carrier: 440 Hz.
- New engine suite passes with optional SLICE enabled and under undefined-behaviour sanitisation. Engine voice state remains 84 bytes.
- Actual menu: initial demo confirmation preserves the working song, cancel clears it, second confirmation loads all three new engines and four-bar patterns without saving a numbered slot. Loading while playing is rejected.
- FIRST LIGHT actual C sequencer rendered 12 seconds of non-silent stereo PCM; MP3 is a listening copy. Screens are captured from the actual C framebuffer.
- Native HTTP preview test and complete firmware preprocessing pass.
- No pi32v2 build, hardware boot, target linker budget or IRQ timing measurement is available.


## ORBIT 0.3.1 LFO correction

LFO source controls have no audio effect while all destination depths are zero; this is intentional and preserves patch defaults. SOURCE/DEST page labels and a zero-depth hint make that condition visible. New-engine SHP destinations now control harmonic balance, pulse width and FM4 depth.

The added actual voice/LFO PCM regression suite verifies that zero-depth renders remain identical when source controls change, that PIT/FLT/SHP/AMP each change PCM on all three new engines, and that RATE/WAVE/PHASE/FADE each change PCM when a destination is enabled. Source-phase and fade tests retrigger a fresh phrase. UI regression checks the second LFO press and KNOB4 destination-depth update. All 13 host programs pass.

## ORBIT 0.4.0: SLOOP 2.4.1 and color styles

Upstream integration source: v2.4.1 / a1c5d68767ae10fafb6821dc63b9b1fc490342d2. Issue #1 is a display/update proposal; the user's follow-up selects optional color styles instead of mandatory monochrome.

- 19 host programs: FM6 (including AMS, all 32 algorithms, envelopes, patch formats and NOR bank), user kits / USR4, stress, UI pages, UAC, user presets, ORBIT DSP/LFO, IRQ mitigation, recovery, event Tape, sequencer, project migration, storage, drums, recording, song audio and UI.
- Existing ORBIT FUN4 engine IDs retained through FUN5 conversion; optional SLICE build retains ORBIT ordering and passes independent-engine checks.
- Tape metadata: nudge, fill and locks survive copy/drop; lift clears metadata; lock-capacity overflow refuses a drop before editing the destination.
- Actual framebuffer captures of ORBIT, PASTEL, NEON and MONO are in docs/orbit-styles-0.4.png. They are firmware renders, not hardware photos.
- Native HTTP preview: 44.1 kHz stereo, finite non-silent sequence audio, panel operations and invalid input rejection.
- Web editor checks: protocol v9, dynamic FM6 engine ID 12, DX7 voices/banks, sample and kit packing, locks/nudges/fills, backup/restore and OTA simulations. Package verification is skipped without a target package.
- The wasm tests exercise eight FM6 factory sounds, the twelve independent patches, FIRST LIGHT, all twelve independent-engine/LFO destination combinations, rapid button queues, autosave reboot, color-style persistence and visualizer selection.
- Memory / divide stress uses AddressSanitizer and integer-divide-by-zero checks for 6000 frames. Leak detection is disabled because the managed process environment prevents LeakSanitizer's /proc inspection; firmware buffers are static.

A broader undefined-behavior sanitizer trial reports inherited signed-left-shift expressions in GRAIN. ORBIT does not claim whole-tree UBSan cleanliness. Negative shifts observed in FM6 velocity/LFO and common DSP DC/chorus/filter paths are expressed as multiplication, preserving intended arithmetic.

The pi32v2 target compile, target RAM/flash budget, IRQ timing, USB enumeration, transfers and real-device boot/audio remain unverified. No installable ORBIT package is shipped. The hardware div0-trap clearing mitigation remains present and host tested.
