# PROJECT KNOWLEDGE BASE - ORBIT

**Generated:** 2026-10-09
**Commit:** 4f2f860
**Branch:** main

## OVERVIEW
FM-1(pi32v2) 대상 C 펌웨어 그루브박스입니다. 단일 번역 단위 펌웨어, Python 빌드·호스트 미리보기 도구, 의존성이 없는 Web MIDI 에디터로 구성됩니다.

## 필수 요구사항 (원래 루트 지침, 약화 금지)
- 펌웨어를 바꾸기 전에 README.md와 BUILDING.md를 읽습니다.
- 소스는 SLOOP 커밋 d691ba7b2d922f1a1f41a3622cffe29ce41c5506에서 파생되었고 upstream v2.4.1(a1c5d68767ae10fafb6821dc63b9b1fc490342d2)이 통합되어 있습니다. GPL 고지와 자산 라이선스를 유지합니다.
- 펌웨어는 C 번역 단위 하나와 작은 strip canvas를 사용합니다. 타깃 메모리 예산을 확인하지 않고 full framebuffer나 PCM tape를 할당하지 않습니다.
- Event tape는 시퀀서 스텝 위의 view/edit layer입니다. 오디오 녹음으로 설명하지 않습니다.
- 펌웨어를 변경하면 `python tools/orbit_check.py`를 실행합니다.
- `python tests/orbit_preview_test.py`는 `tools/orbit_preview.py`로 `tools/orbit_host.c`를 빌드한 뒤 실행합니다.
- 툴체인과 SDK가 있으면 펌웨어 변경에 pi32v2 타깃 컴파일과 `tests/target_budget.py`도 필요합니다. 호스트 테스트로 타깃 안전성이나 타이밍을 입증할 수 없습니다.
- 웹 미리보기 소리는 펌웨어 C 엔진에서 나오도록 유지합니다. 호스트 미리보기의 project-save 더블은 의도적으로 비영구적이며, 이 한계를 명확히 표시합니다.
- 실제로 수행하지 않은 Dots 배포나 하드웨어 테스트를 주장하지 않습니다.

## STRUCTURE
```
orbit/
├── firmware/
│   ├── src/      # unity build 앱 본체 (61개 파일, 약 21.3k LOC) - 하위 AGENTS.md
│   ├── hal/      # 헤더 전용 fm1_* HAL + ISR/벡터 .S - 하위 AGENTS.md
│   ├── loader/   # OTA 업데이트 로더 (4개 파일)
│   ├── app.ld, crt0.S
├── tools/        # build.py, 생성기, 미리보기, 설치 - 하위 AGENTS.md
├── tests/        # 호스트 테스트, 러너, 기준값 - 하위 AGENTS.md
├── web/          # editor.html, EDITOR_PROTOCOL.md, fm1ota.js, fm1pkg.js (루트가 담당)
├── preview/      # index.html: 네이티브 미리보기 UI (영어 기본)
├── docs/         # VALIDATION.md, OP1-ENGINES.md, 이미지, mp3
├── assets/       # fonts, samples-cc0, icons
├── LICENSES/, LICENSE, LICENSING.md
├── README.md, BUILDING.md, GUIDE.md, README-SLOOP.md, DOTS.md
└── build.sh, requirements.txt (Pillow)
```

## WHERE TO LOOK
| 작업 | 위치 | 비고 |
|------|------|------|
| 펌웨어 진입/include 순서 | firmware/src/felucca.c | include 순서가 의미를 가짐 |
| 시퀀서, 키 입력 | firmware/src/seq.c | 2262줄 핫스팟 |
| SysEx 에디터 프로토콜 | firmware/src/editor.c ↔ web/editor.html | 명세는 web/EDITOR_PROTOCOL.md |
| USB | firmware/src/usb.c | 1142줄 |
| 저장/로드 (FUN5) | firmware/src/project.c | |
| 하드웨어 추상화, ISR | firmware/hal/ | |
| 생성 헤더 | tools/build.py `generate()` | build/gen에 출력 |
| 호스트 미리보기 | preview/index.html, tools/orbit_preview.py, tools/orbit_host.c | |
| 호스트 C 테스트 하네스 | tests/hostsim.c, tests/orbit_host_support.h | |
| 검증 상태 기록 | docs/VALIDATION.md, README.md | |

## CODE MAP
참조 수는 grep 기준입니다. **LSP 참조 수는 측정하지 않았습니다(unmeasured).**

| 심볼 | 종류 | 위치 | 참조(grep) | 역할 |
|------|------|------|-----------|------|
| felucca.c | 진입 TU | firmware/src/felucca.c | - | 단일 번역 단위 |
| trk / song | 전역 | firmware/src/core.h:286-287 | 639 / 686 | 트랙 4개(신스 3+드럼), 곡 상태 |
| fm1_irq_off | 함수 | firmware/hal/fm1_irq.h | 39 | ISR 임계 구역 |
| ui_draw | 함수 | firmware/src/ui_draw.c (main.c:266에서 호출) | 20 | 화면 렌더 |
| keyboard_block | 함수 | firmware/src/seq.c:1543 | 10 | 키/레이어 입력 |
| project_load / project_save | 함수 | firmware/src/project.c | 10 / 9 | FUN5 저장 |
| felucca_init | 함수 | firmware/src/main.c:100 | 5 | 부트 |
| seq_tick | 함수 | firmware/src/seq.c:1915 | 3 | 시퀀서 틱 |
| ed_handle | 함수 | firmware/src/editor.c:584 | 3 | SysEx 처리 (ED_PROTO=9) |
| hostsim.c | 하네스 | tests/hostsim.c | C 테스트 20개가 include | 호스트 렌더 |
| build.generate | 함수 | tools/build.py:91 | orbit_check, orbit_preview, build_windows | 헤더 생성 |

## CONVENTIONS
- 펌웨어는 felucca.c가 .c 파일들을 include하는 unity build입니다. 새 파일은 include 순서에 맞춰 넣습니다.
- 웹 에디터는 의존성 없는 단일 파일(web/editor.html, 6079줄)입니다. `?mock=1`로 하드웨어 없이 실행합니다.
- 에디터 SysEx는 "FL" 헤더, pack7 인코딩, 프로토콜 v9를 사용합니다. editor.c와 editor.html, EDITOR_PROTOCOL.md를 함께 바꿉니다.
- 미리보기 경로: preview/index.html ↔ tools/orbit_preview.py(HTTP /api) ↔ tools/orbit_host.c(stdin 명령) ↔ tests/orbit_host_support.h(테스트와 공유).
- SPDX 헤더를 유지합니다.

## ANTI-PATTERNS (이 프로젝트)
- 제거된 기능을 되살리지 않습니다: 블루투스 헤드폰, PCM tape 녹음, SLOOP 2.3 호환(FUN5).
- 미리보기 데모 패턴을 펌웨어 동작으로 소개하지 않습니다. 데모 패턴은 미리보기 전용입니다.
- 미리보기에서 샘플 업로드를 전제하지 않습니다. 지원하지 않습니다.
- 타깃 빌드나 FM-1 검증을 수행한 것처럼 기록하지 않습니다.

## 출처와 라이선스
- isod89/sloop-fm1 d691ba7b2d922f1a1f41a3622cffe29ce41c5506(v2.3)의 스냅샷에 v2.4.1(a1c5d68767ae10fafb6821dc63b9b1fc490342d2)을 통합했습니다(0.4.0). 히스토리를 가진 포크가 아닙니다.
- 코드는 GPL-3.0-only이고, fm6_core.c만 Apache-2.0(msfa)입니다.
- icons.png와 gen_waves 드럼은 Hügelton 소유입니다(GPL §7). samples-cc0는 CC0, Terminus는 SIL OFL, fukiai.ttf는 MIT입니다.

## COMMANDS
```bash
python tools/orbit_check.py                 # SDK 없이 가능: build.generate() 후 CASES 19개 → build/host/orbit-checks.txt
python tools/orbit_preview.py --port 8080   # cc로 tools/orbit_host.c 빌드 (--host, --no-build)
python tests/orbit_preview_test.py          # 미리보기를 먼저 빌드한 뒤 실행
sh build.sh [--release X.Y]                 # 툴체인/SDK/Pillow 검사 후 tools/build.py, macOS는 Docker 필요
sh tests/run_tests.sh                       # build/orbit.fwsc가 없으면 즉시 종료
python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt
node web/test_web.mjs
```
환경변수: `JIELI_TOOLCHAIN`(기본 ~/.jieli/toolchain), `AC79_SDK`(기본 ~/fw-AC79_AIoT_SDK).

## NOTES
- 현재 상태: pi32v2 빌드와 `build/orbit.fwsc` 생성은 검증했습니다. 실기기 검증과 CPU 예산 실패의 상세 상태는 `docs/VALIDATION.md`를 확인합니다.
- `web/make_site.py`와 독립 웹 설치 페이지는 저장소에 없습니다. 설치에는 `tools/fm1_install.py`를 사용합니다.
- 미리보기 오디오 버퍼는 250~550 ms입니다.
- 핫스팟: seq.c 2262줄, usb.c 1142줄, editor.c 1110줄, web/editor.html 6079줄, web/test_web.mjs 1102줄.
