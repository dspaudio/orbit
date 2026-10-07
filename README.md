# ORBIT 0.2.1 — FM-1 groovebox firmware

SLOOP/Felucca의 실제 C 음원 엔진 위에 이벤트 Tape 중심의 작업 흐름을 추가한 개발 버전입니다. OP-1의 네 색상 노브와 모드별 편집 흐름에서 영감을 얻었으며, OP-1 화면·음원·로고를 복제하지 않습니다.

## 0.2.1 부팅 예외 설정 수정

Felucca #61 / #111의 컴파일러 관련 부팅 장애 완화 조치를 적용했습니다. `fm1_irq_init`에서 EMU_CON의 bit 2를 명시적으로 꺼서, 이전 부팅에서 남아 있던 0으로 나누기 예외 설정도 해제합니다. 다른 비트와 예외·벡터·분기 추적 설정은 유지합니다. 이 설정은 실제 산술 오류를 해결하는 조치가 아니며, 실기기 부팅 안전성을 보장하지 않습니다.

`irq_init_test`는 Linux에서 메모리로 대체한 레지스터에 실제 초기화 함수를 실행해 cold/warm 상태의 비트 해제와 다른 설정 보존을 검사합니다. 전용 컴파일러 동작이나 실기기 레지스터 동작을 검증하는 테스트는 아닙니다.

## 0.2 화면 업데이트

OP-1의 그래픽 중심 화면 흐름을 참고해 Tape의 두 릴, 신스의 네 색상 파형/오비트, 엔벌로프의 네 색상 곡선, 믹서의 네 채널 페이더를 추가했습니다. 기존 노브의 파라미터 연결을 유지하며 화면 요소를 해당 색상에 맞췄습니다. 엔진 그래픽은 파라미터를 시각화한 그림이고, SAMPLE/GRAIN의 output 파형만 실제 마스터 오디오를 표시합니다. 원본 샘플 파일의 편집용 파형은 아닙니다.

화면 참조: https://teenage.engineering/guides/op-1/original/synthesizer-mode 및 https://teenage.engineering/guides/op-1/original/tape-mode

![ORBIT 0.2 screens](docs/orbit-preview-0.2.png)

## 구현 상태

| 기능 | 현재 동작 |
|---|---|
| HOME / Tape | 네 트랙 이벤트 타임라인, 재생 위치, 녹음 표시, 구간 선택 |
| Tape 편집 | COPY / LIFT, 다른 위치로 DROP, 패턴 끝에서 잘라 붙이기, 신스/드럼 타입 검사 |
| 신스 | 기존 9개 엔진 및 음색·엔벌로프·LFO·FX 편집 재사용 |
| 샘플러 | 기존 사용자 슬롯 3개 및 웹 편집기의 가져오기·CHOP·실기기 업로드 재사용 |
| 시퀀서 | 기존 64스텝·라이브 녹음·코드·래칫·드럼 레인·송 배열 재사용 |
| 믹서 | 기존 GLO 트랙 레벨·팬·뮤트·솔로·이펙트 재사용 |
| 저장 | 기존 FUN4 프로젝트 형식 유지. 클립보드와 Tape 선택 범위는 일시 상태 |
| 호스트 미리보기 | 펌웨어와 같은 C DSP·화면·시퀀서를 브라우저에서 조작 |

Tape에는 음표와 드럼 이벤트를 저장합니다. OP-1처럼 긴 오디오를 녹음하거나 테이프 속도에 따라 녹음된 오디오의 피치를 바꾸는 기능은 구현하지 않았습니다. 블루투스 출력도 이번 버전에 포함하지 않았습니다.

## 빠른 실행

Python 3, C 컴파일러(gcc/cc), Pillow가 필요합니다. 테스트의 브라우저 스크립트 문법 검사에는 Node.js를 사용합니다.

```bash
python -m pip install -r requirements.txt
python tools/orbit_preview.py --port 8080
```

브라우저에서 http://127.0.0.1:8080 을 열고 **오디오 시작**을 누릅니다. 미리보기에는 기본 데모 패턴이 있습니다. 실기기 펌웨어의 부팅 프로젝트에는 데모를 넣지 않습니다.

같은 펌웨어 DSP를 사용하지만, 브라우저 오디오는 약 250–550ms를 버퍼링하므로 실기기의 연주 지연을 평가하는 용도는 아닙니다. 미리보기의 저장/불러오기는 테스트 대역이며 프로젝트를 파일로 보존하지 않습니다. 사용자 샘플을 미리보기 프로세스에 업로드하는 경로도 제공하지 않습니다.

## FM-1 조작

| 조작 | HOME / 이벤트 Tape |
|---|---|
| KNOB 1 | 붙일 위치(head) |
| KNOB 2 / 3 | 구간 시작(in) / 끝(out), 양 끝 포함 |
| KNOB 4 | COPY / LIFT 선택 |
| OCT− | 선택 구간 복사 또는 들어내기 |
| OCT+ | head 위치에 클립보드 붙이기. 기존 이벤트 덮어쓰기 |
| ALGORITHM | 트랙 선택 |
| SELECT | BPM |
| PLAY / REC | 기존 재생/라이브 녹음 동작 |
| EDIT / SEQ / GLO | 음원 파라미터 / 스텝 편집 / 믹서 |
| HOME 길게 | 기존 설정 메뉴 |

신스 트랙 사이에는 복사할 수 있고, 드럼 클립은 드럼 트랙에만 붙입니다. 신스 LIFT는 기존 EDIT+OCT− undo 대상으로 표시됩니다. 드럼 LIFT는 클립보드를 DROP해서 복구할 수 있습니다. 다른 클립을 복사하면 이전 클립보드를 대체합니다. HOME 이외의 화면에서는 기존 OCT± 조작을 유지합니다.

샘플 추가 및 편집은 `web/editor.html`의 기존 샘플/CHOP 기능을 사용합니다. 웹 MIDI로 실기기를 연결하려면 기존 SLOOP 편집기 지침과 브라우저 조건을 따르세요. 자세한 원본 설명은 [README-SLOOP.md](README-SLOOP.md)에 있습니다.

## 검증

```bash
python tools/orbit_check.py
python tools/orbit_preview.py --port 8080  # 최초 네이티브 미리보기 빌드
# 위 서버를 Ctrl+C로 종료한 뒤:
python tests/orbit_preview_test.py
```

`orbit_check.py`는 하드웨어 SDK 없이 Tape 편집/패널 입력, 시퀀서, 프로젝트 형식, 플래시 저장 로직, 드럼, 녹음, FX, 송 오디오와 화면을 검사합니다. 결과는 `build/host/`에 저장됩니다. 미리보기 HTTP 테스트는 같은 프로세스 트리 안에서 서버를 시작하고 종료합니다.

## 실기기 빌드

원본 [BUILDING.md](BUILDING.md)의 JieLi pi32v2 Linux 툴체인과 AC79 SDK 파일이 필요합니다.

```bash
export JIELI_TOOLCHAIN=/path/to/jieli/toolchain
export AC79_SDK=/path/to/AC79_SDK
python tools/build.py
```

기존 빌드 파이프라인의 출력 이름 `build/felucca.fwsc`를 유지합니다. 이 배포에는 설치 파일을 포함하지 않습니다. 전용 툴체인/SDK가 없는 환경에서 소스와 호스트 테스트를 작성했으며, 타깃 컴파일·플래시/RAM 사용량·오디오 ISR 부하·실기기 패널·USB 업로드 검증은 아직 수행하지 않았습니다.

## 출처와 라이선스

기반: [isod89/sloop-fm1](https://github.com/isod89/sloop-fm1), 커밋 `d691ba7b2d922f1a1f41a3622cffe29ce41c5506`. 소프트웨어는 GPL-3.0-only이며 원저작권 표시와 라이선스 파일을 보존했습니다. 폰트와 샘플의 별도 라이선스는 [LICENSING.md](LICENSING.md)와 각 assets 디렉터리의 표시를 따릅니다. 원본 전체 Git 이력의 fork가 아닌, 고정된 소스 스냅샷에서 시작한 독립 개발 저장소입니다.
