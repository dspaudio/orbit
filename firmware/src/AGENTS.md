# firmware/src

## OVERVIEW
FM-1 앱 펌웨어 본체(61개 파일, 약 21.3k LOC C). 점수 17(파일 수·중심성·심볼 밀도 높음)이라 별도 지침을 둡니다.

## 빌드 구조 (unity build)
- 유일한 진입 번역 단위는 `felucca.c`입니다. HAL 헤더 → `libc.c` → `lcd.c` → `gfx.c` → `engines.c` → ... → `main.c` 순서로 `#include`하며, 주석대로 **순서가 의미를 가집니다**.
- 중첩 include 관계(파일을 옮기거나 이름을 바꿀 때 함께 확인):
  - `engines.c` → `dsp.c`, `eng_analog.c`, `eng_digital.c`, `eng_phase.c`, `eng_lofi.c`, `eng_sample.c`, `eng_formant.c`, `eng_trio.c`, `eng_drawbar.c`, `eng_grain.c`, `eng_fm6.c`(→`fm6_core.c`), `eng_slice.c`, `eng_orbit.c`
  - `drums.c` → `drum_synth.c`, `fx.c` → `punch.c`, `editor.c` → `editor_fm6.c`
  - `project.c` → `arranger_scene.c`, `fm6_bank.c`
  - `ui_draw.c` → `orbit_modes.c`, `ui_input.c` → `orbit_demo.c`, `ui_studio.c` → `orbit_ui.c`
- 새 `.c`는 Makefile이 아니라 위 include 사슬에 넣어야 컴파일됩니다. 심볼은 대부분 `static`이며, 전역 상태는 `core.h`의 `static track_t trk[NTRK]`(NTRK=4: 신스 3 + 드럼 1), `static song_t song`입니다.
- 기능 플래그: `FELUCCA_FLASH`, `FELUCCA_OTA`, `FELUCCA_OTA_DRYRUN`, `FELUCCA_CDC`, `FELUCCA_UART`, `FELUCCA_ICONS`, `FELUCCA_SLICE`(tools/build.py가 설정). `eng_slice.c`/`slice_test.c`는 `FELUCCA_SLICE=1`일 때만 의미가 있습니다.
- 생성 헤더(`build/gen/felucca_tables.h`, `felucca_fm6.h`, `felucca_font.h` 등)에 의존합니다. 손으로 편집하지 말고 `tools/gen_*.py`를 고칩니다. ORBIT 부트 화면은 `splash.c`가 직접 렌더링하며 SLOOP 로고 헤더를 생성하지 않습니다.

## WHERE TO LOOK
| 작업 | 위치 | 비고 |
|------|------|------|
| 부트·메인 루프 | `main.c` (`felucca_init`, `ui_draw()` 호출) | `recovery.c`는 초기 USB 업데이터, 신스 없음 |
| 오디오 ISR·믹스 | `audio.c` `audio_block()` → `fx.c` | Q15 → 24bit |
| 시퀀서·키보드·녹음·undo | `seq.c` (2262줄, 최대 핫스팟) | `seq_tick`, `keyboard_block`, 오디오 ISR 문맥 |
| 보이스 할당·글라이드·모노 | `voice.c` `voice_alloc` | 8 voice 예산, FM6는 6 voice cap |
| 엔진 추가/수정 | `engines.c` 사슬 + `eng_*.c` | ORBIT 독자 엔진은 `eng_orbit.c`(SWARM/PULSE/FM4) |
| FM6 | `eng_fm6.c`, `fm6_core.c`, `fm6_bank.c`, `editor_fm6.c` | `fm6_core.c`만 Apache-2.0(msfa) |
| 웹 에디터 SysEx | `editor.c` `ed_handle` | 프로토콜 버전 `ED_PROTO`(=9), 명세는 `web/EDITOR_PROTOCOL.md` |
| 프로젝트 저장 형식 | `project.c` | FUN5 저장, FUN1~FUN4 읽기 시 기본값 채움 |
| 사용자 프리셋 | `upreset.c` | RAM 미러, 플래시는 `FELUCCA_FLASH` |
| USB MIDI/CDC/UAC | `usb.c` `usb_poll` | TIMER5 ISR 2 kHz |
| 화면 | `ui.c`, `ui_draw.c`, `ui_studio.c`, `ui_layers.c`, `ui_vis.c`, `ui_menu.c`, `ui_song.c` | 시그니처 캐시로 재그리기 억제 |
| 이벤트 Tape | `orbit_tape.h`, `orbit_ui.c`, `orbit_modes.c` | COPY/LIFT/DROP |

## CONVENTIONS
- ISR과 공유하는 상태는 `fm1_irq_off()` / `fm1_irq_on()`으로 감쌉니다(firmware·tests·tools 전체 39곳에서 참조).
- 타깃 코드에 `malloc`/`free`·ISR 내 `printf` 금지. 버퍼는 정적 고정 크기(예: `ed_out[600]`, 범위 검사 `ed_n < sizeof ed_out - 1u`).
- 저장 형식이나 SysEx 기능을 바꾸면 `project.c`의 버전 상수(FUN 매직, `PROJ_NP_V*`) 또는 `ED_PROTO`를 올리고 구형 읽기 경로를 유지합니다. FUN5 파일은 SLOOP 2.3에서 읽을 수 없습니다.
- enum 접두사: `KS_*`, `LY_*`, `FC_*`, `ED_*`. 타입은 `*_t`(`track_t`, `voice_t`, `step_t`, `preset_t`, `song_t`).
- DSP는 정수 연산이며 부호 있는 left-shift·FM6 위상 wrap에 의존합니다. 테스트 sanitizer가 `shift-base,signed-integer-overflow`를 제외하는 이유이므로 "수정"하지 마십시오.
- 파일 헤더의 SPDX/Copyright 줄을 유지합니다.

## ANTI-PATTERNS
- DSP 루프 안의 나눗셈 추가: `tests/target_budget.py`가 렌더 루프 명령 수를 +10%까지만 허용합니다.
- 소리가 바뀌는 변경 후 `tests/golden.txt`를 검토 없이 갱신하기.
- 블루투스 헤드폰·PCM tape 녹음 재도입(제거된 기능).
