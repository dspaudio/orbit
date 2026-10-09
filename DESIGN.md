# ORBIT: 오리지널 OP-1 조작과 화면

## 1. 분위기와 정체성

기준은 OP-1 field가 아닌 [오리지널 OP-1 공식 가이드](https://teenage.engineering/guides/op-1/original)이다. 검은 화면 위의 얇은 선, 의미 있는 기계 도해, 파랑·초록·흰색·주황 encoder 순서가 핵심이다. Orbit 이름과 기존 엔진을 유지하며 원본의 로고나 그림 자산은 복제하지 않는다.

공식 Synth envelope 도해의 viewBox는 `140×70`이다. FM-1의 실제 `240×240` LCD에서는 가로 그래픽 영역과 보조 조작 안내를 구분한다. 전체 framebuffer를 추가하지 않고 기존 `240×124` strip canvas를 사용한다.

## 2. 색

| 역할 | 기존 토큰 / 값 | 용도 |
|---|---|---|
| 화면 | `C_BLACK`, `#000000` | 실제 OLED 도해의 검은 바탕 |
| 첫 encoder | `TE_COL[0]`, RGB(40,124,255) | blue |
| 둘째 encoder | `TE_COL[1]`, RGB(30,204,112) | green |
| 셋째 encoder | `TE_COL[2]`, RGB(255,255,255) | white: 기존 ORBIT 팔레트의 yellow를 교정 |
| 넷째 encoder | `TE_COL[3]`, RGB(255,98,26) | orange |
| 비활성 | `TE_DIM`, `TE_MID` | 기존 색의 밝기 조정 |
| 보조 선과 글자 | `TE_G1`–`TE_G4`, `pal` | 기존 중립 회색 |
| 녹음·삭제 | `te_alert` | 기존 경고 표시 |

PASTEL·NEON·단색 접근성 팔레트는 사용자 선택으로 보존한다. 웹은 같은 네 encoder 순서를 사용하고 track 색과 encoder 역할을 혼동하지 않는다.

## 3. 글자

펌웨어는 기존 생성 글꼴 `FONT_S`와 `FONT_L`을 유지한다. 그래픽에 불필요한 큰 숫자를 넣지 않고 제목·값·보조 안내의 위계를 분리한다. 웹 에디터는 기존 내장 `SLOOP Terminus` 16px/32px와 `Fukiai` 아이콘, Orbit 워드마크의 시스템 sans를 유지한다. 네이티브 미리보기는 기존 시스템 sans를 유지한다. 새 외부 글꼴을 요청하지 않는다.

## 4. 간격과 배치

LCD는 4개의 60px 열을 유지한다. 선은 1px, inset은 4px 배수다. 중앙 그래픽을 먼저 읽고 하단에서 현재 네 encoder의 실제 역할을 확인한다. 웹은 기존 반응형 레이아웃 안에서 주요 모드와 네 모듈을 분리하며 390px에서도 주요 조작에 가로 스크롤이 생기지 않게 한다.

## 5. 공통 구성 요소와 조작 계약

### 주요 모드

Synth / Drum / Event Tape / Mixer를 구분한다. 실제 FM-1 입력을 기준으로 Synth는 선택한 synth track의 EDIT, Drum은 drum track과 기존 drum 페이지, Tape는 HOME, Mixer는 GLO이다. 기존 트랙 선택·transport·SEQ·SCL·ARP·SAVE·hold layer를 제거하지 않는다.

### 네 sound 모듈

원본의 T1/T2/T3/T4는 engine/envelope/effect/LFO이다. FM-1에서는 EDIT/ENV/FX/LFO가 이에 대응한다. 물리 키 이름과 OP-1 역할을 함께 안내한다. 기존 여러 parameter page의 접근은 보존한다. 버튼 반복이 페이지 순회라면 이를 가짜 enable toggle로 소개하지 않는다.

### 네 encoder

항상 blue/green/white/orange 순서다. Envelope의 기본 페이지는 A/D/S/R이다. Mixer의 level 화면은 각각 track 1/2/3/4 level을 조절하며 drum level은 실제 `G_DRLVL`을 사용한다. 현재 조작이 pan이면 네 track pan을 표시한다. 화면 도해와 실제 파라미터 변경이 일치해야 한다.

### Engine와 sound 선택

Engine-only 변경은 envelope/FX/LFO를 보존하는 기존 경로를 사용한다. Sound preset 선택은 기존 전체 sound 경로를 사용한다. 지원하지 않는 Shift browser나 sound slot을 활성 버튼처럼 추가하지 않는다.

### Event Tape

두 reel과 네 track timeline은 실제 sequencer 이벤트를 나타낸다. Lift/Drop/Copy/Paste는 기존 이벤트·nudge·fill·lock을 보존한다. 표시 이름은 Event Tape이며 PCM 녹음, tape speed/pitch, audio overdub, album 기능을 있는 것처럼 표시하지 않는다.

### 웹 조작 상태

모드·모듈 선택은 실제 기존 editor 또는 C host 경로를 호출한다. 선택 상태는 글자·명암·`aria-pressed`로 표시한다. 버튼은 hover/pressed/focus/disabled 상태가 명확해야 한다. 비연결 상태에서 실제 MIDI 작업 성공을 주장하지 않는다. preview의 project save는 비영구적인 host double이라는 안내를 유지한다.

## 6. 움직임과 피드백

LCD의 lazy redraw와 실제 transport/parameter animation을 유지한다. 웹의 버튼 피드백은 100–150ms, 기존 패널 전환은 200ms 이내다. 새 layout animation을 추가하지 않는다. `prefers-reduced-motion`을 존중한다. 비동기 작업의 실패는 실제 오류로 표시하고 임의 대기 시간을 성공 판정으로 사용하지 않는다.

## 7. 깊이와 표면

LCD는 검은 평면과 얇은 선만 사용한다. 웹의 기존 기기 shell은 유지하고 새 장식 카드·그라데이션·그림자를 추가하지 않는다. 모드·모듈은 동등한 크기의 실제 버튼이다. 장식용 OP-1 모사 컨트롤을 추가하지 않는다.

## 8. 접근성과 검증 범위

웹은 키보드 접근, visible focus, 읽을 수 있는 contrast, 실제 control label을 유지한다. 모바일·데스크톱 실제 렌더와 버튼·encoder·transport·편집 경로를 검증한다. 펌웨어는 실제 host C 캡처와 pi32v2 빌드·메모리·기존 예산 검사로 확인한다. 이 증거는 FM-1 실기기 timing이나 플래싱 시험을 대신하지 않는다.
