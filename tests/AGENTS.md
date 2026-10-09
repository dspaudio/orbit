# tests

## OVERVIEW
호스트 테스트 모음(42개 파일: C 테스트 약 30개, Python 4개, 셸 러너, 기준값 txt). 점수 11이고, 실제 펌웨어 소스를 호스트에서 컴파일하는 별도 검증 영역이라 지침 파일을 둡니다.

## 구조
- 테스트는 펌웨어 `.c`를 직접 include해 컴파일합니다(DSP·시퀀서·UI는 실제 코드, 디스크·BLE·ADC·플래시는 스텁).
- `hostsim.c`: 오디오 렌더 하네스. C 테스트 20개가 include하는 중심 파일입니다. 단독 실행은 `build/host/hostsim ENGINE PRESET MONO OUT.wav`이고, 환경변수 `SECS`, `CHORD`, `DRUMS`, `TRACKS=DIR` 등을 받습니다.
- `orbit_host_support.h`: 실제 UI(ui*.c)와 패널·플래시·저장소 더블. `tools/orbit_host.c`(웹 미리보기)도 이 헤더를 공유하므로 수정하면 미리보기에도 영향이 갑니다.
- 기준값 파일: `golden.txt`(렌더 해시 107줄), `cpu_baseline.txt`(+25%, 타이밍 측정 시 +35%), `target_budget.txt`(`build/felucca.dis`에서 셈한 렌더 루프 명령 수, +10%).

## 두 러너의 차이
| 러너 | 전제 | 범위 |
|------|------|------|
| `python tools/orbit_check.py` | 벤더 SDK 불필요. `build.generate()`로 `build/gen` 생성 | `CASES`의 19개 프로그램, 3병렬, 로그 `build/host/<name>.log`, 요약 `build/host/orbit-checks.txt` |
| `tests/run_tests.sh` | `build/felucca.fwsc`가 없으면 즉시 종료("run ./build.sh first"), 즉 타깃 빌드가 필요 | 전체: storage/recovery/OTA/loader, regress, `target_budget.py`, `install_test.py`, `node web/test_web.mjs`(node가 있을 때) |
| `python tests/orbit_preview_test.py` | 먼저 미리보기를 한 번 빌드(`--no-build`로 서버 기동) | HTTP 브리지와 네이티브 오디오 |

- 개별 빌드 형식: `cc -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal -o build/host/<t> tests/<t>.c -lm`. 예외로 `uac_test`는 `-DHALF_FRAMES=256 -DT_CDC=1`이 필요하고, `orbit_test`는 출력 디렉터리 인자를 받습니다.
- `slice_test.c`는 `FELUCCA_SLICE=1` 구성에서만 의미가 있습니다.

## 환경변수 (run_tests.sh / regress)
`GOLDEN_UPDATE=1`(golden 재계산), `BUDGET_UPDATE=1`(cpu_baseline·target_budget 재작성), `VERBOSE=1`, `JOBS=n`(기본 8), `CPU_VALGRIND`/`SKIP_CPU_VALGRIND`(valgrind가 있으면 callgrind를 자동 사용), `CC_UB`(ASan/UBSan 컴파일러), `STRESS_FRAMES`, `SOAK_MIN`.

## CONVENTIONS
- 실패는 `check(cond, "msg")` 누적 방식이나 `assert()`로 처리합니다. 렌더마다 fork한 자식 프로세스에서 깨끗한 상태로 실행합니다.
- 산출물(WAV/PPM)은 `build/host/`에 씁니다. 저장소 안의 다른 위치에 쓰지 않습니다.
- ASan 구성은 `-fno-sanitize=shift-base,signed-integer-overflow`를 의도적으로 유지합니다(펌웨어 DSP가 의존).
- 새 C 테스트는 `run_tests.sh`에 추가하고, 호스트 필수 검사라면 `orbit_check.py`의 `CASES`에도 추가합니다.

## ANTI-PATTERNS
- `GOLDEN_UPDATE`/`BUDGET_UPDATE` 결과를 diff 검토 없이 커밋하기. 기준값은 반드시 원인이 된 변경과 함께 커밋합니다.
- 고정 sleep으로 타이밍을 맞추기. 테스트는 고정 블록 단위로 진행합니다.
- 패키지 검사가 건너뛰어졌는데(타깃 `.fwsc` 없음) 통과로 보고하기.
