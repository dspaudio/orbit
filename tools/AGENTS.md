# tools

## OVERVIEW
빌드 드라이버, 생성기, 호스트 미리보기, 설치 도구(27개 파일, Python 24 + C 1 + sh 1 + JSON 1). 점수 8이고, 빌드·생성 파이프라인이라는 별도 영역이라 지침 파일을 둡니다.

## WHERE TO LOOK
| 작업 | 파일 | 비고 |
|------|------|------|
| 타깃 빌드 | `build.py` (`./build.sh`가 Pillow·툴체인·SDK·Docker를 확인한 뒤 exec) | `--release X.Y`; `FELUCCA_*` 플래그와 `FELUCCA_VERSION`(기본값은 `firmware/src/ui.c`) |
| Windows 빌드 | `build_windows.py` | `build.generate()` 재사용 |
| 생성 헤더 | `build.py` `generate()` | `build/gen/`에 `felucca_font.h`, `felucca_icons.h`, `felucca_tables.h`, `felucca_samples.h`, `felucca_drumkits.h`, `felucca_fm6.h`를 병렬 생성 |
| 개별 생성기 | `gen_font.py`, `gen_icons.py`, `gen_tables.py`, `gen_samples.py`, `gen_drumkits.py`, `gen_fm6_patches.py`, `gen_logo.py`, `gen_waves.py`, `gen_webfont.py`, `gen_builtin_hiphop.py`, `gen_hiphop_pack.py` | |
| 레벨 보정 | `level_drumkits.py`, `level_presets.py`, `drumkit_levels.json`(579줄 생성 데이터) | |
| 호스트 검사 | `orbit_check.py` | 19개 `CASES`; 내용은 `tests/AGENTS.md` 참고 |
| 웹 미리보기 | `orbit_preview.py` + `orbit_host.c` | `--port`(기본 8080), `--host`(기본 127.0.0.1), `--no-build` |
| 설치·업로드 | `fm1_install.py`(`--info`, `MidoLink`, `pack7/unpack7`), `fm1_sample_upload.py`, `fm1pkg_make.py`(`.fwsc` 패키지) | 실기기 필요 |
| 샘플·압축 | `fetch_cc0.py`(CC0 샘플을 `assets/samples-cc0/`에 받음, 없어도 빌드 가능), `sampleio.py`, `lz4blk.py` | |
| 툴체인 설치 | `get_toolchain.sh` | 기본 경로 `~/.jieli/toolchain` |

## 미리보기 파이프라인
- `orbit_preview.py build()`: `generate()` → `cc -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal tools/orbit_host.c -o build/host/preview/orbit_host -lm`.
- `orbit_host`는 stdin 명령(`button N`, `knob N D`, `render N NOTES`)을 받아 `chunk.wav`와 PPM 화면을 출력하고, Python HTTP 서버(`/api`)가 이를 `preview/index.html`에 넘깁니다. 화면 PNG 변환에 Pillow가 필요합니다.
- 데모 패턴은 `orbit_host.c`에만 있습니다. 펌웨어는 빈 프로젝트로 부팅합니다.
- 브라우저 오디오 버퍼는 약 250~550 ms입니다. 기기 성능이나 지연 근거로 쓰지 않습니다. 샘플 업로드는 미리보기에서 지원하지 않습니다.

## CONVENTIONS
- 생성 결과(`build/gen/*`)는 커밋하지 않습니다. 소리나 표가 바뀌어야 하면 생성기를 고친 뒤 `GOLDEN_UPDATE` 절차를 밟습니다.
- 환경변수: `JIELI_TOOLCHAIN`(기본 `~/.jieli/toolchain`), `AC79_SDK`(기본 `~/fw-AC79_AIoT_SDK`), `PYTHON`, `CC`. macOS·비x86 호스트에서는 Docker가 실행 중이어야 합니다.
- 의존성은 `requirements.txt`(Pillow)이고, 설치 도구는 mido 계열 MIDI 링크를 사용합니다.
- 각 스크립트의 SPDX `GPL-3.0-only` 헤더를 유지합니다.

## ANTI-PATTERNS
- `orbit_host.c`에서 JS나 별도 합성기로 소리를 만들기. 소리는 `mix_block()`(펌웨어 C)에서만 나옵니다.
- `web/make_site.py`가 있다고 가정하기. BUILDING.md에는 언급되지만 이 저장소에서는 추적되지 않습니다.
