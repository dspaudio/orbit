# Dots 작업 디렉터리 구성

이 압축파일에는 ORBIT 소스와 로컬 Git 저장소(.git)가 포함되어 있습니다. 이 대화에서 Dots에 접근할 연결 기능을 사용할 수 없어, Dots 내부에 저장소를 실제로 생성하거나 원격으로 push하지는 않았습니다.

Dots에서 소스 폴더를 작업 디렉터리로 준비한 뒤 아래 명령으로 현재 상태를 확인하고 이어서 개발할 수 있습니다.

```bash
cd fm1-orbit
git status
git log -1 --oneline
python -m pip install -r requirements.txt
python tools/orbit_check.py
python tools/orbit_preview.py --port 8080
```

실기기 설치 버전을 만들 때는 BUILDING.md의 JieLi 툴체인과 AC79 SDK를 먼저 구성하세요. 실제로 타깃 빌드가 성공한 뒤 용량 검사와 FM-1 하드웨어 테스트를 진행해야 합니다. Dots에서 웹 미리보기를 외부로 보여주는 방법은 해당 실행 환경의 포트 노출 기능에 맞춰 설정해야 합니다. 서버는 기본적으로 localhost에만 바인딩합니다.

다음 개발 목표: 타깃 빌드와 자원 예산 검증, 샘플러 편집 화면 통합, Tape 선택 구간의 반복 재생. 블루투스는 별도의 A2DP/SBC 및 HAL 통합 작업으로 남아 있습니다.
