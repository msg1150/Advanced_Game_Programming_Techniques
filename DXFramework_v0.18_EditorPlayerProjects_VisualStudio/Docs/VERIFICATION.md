# v0.18 검증 기록 — 2026-10-26

## 완료한 자동 검사

| 검사 | 결과 |
|---|---|
| DXRuntime.lib + DXEditor.exe + DXPlayer.exe, MSVC v143 Debug x64 컴파일·링크 | 통과 |
| 한글·공백·&·괄호가 포함된 소스 경로에서 동일 빌드 | 통과 |
| 한글·공백·&·괄호가 포함된 출력 경로 | 통과 |
| 기존 DXLegacyDemo 별도 컴파일·링크 | 통과 |
| WorldTerrain/TerrainQuery를 제외한 Player 컴파일·링크 | 통과 |
| test_project_unicode | 통과 |
| test_process_unicode + 별도 인자 검증 helper | 통과 |
| test_unicode_runtime (창 없는 WARP) | 통과 |
| 한글 경로 Player --check-project + 한글 .dxproject 읽기 | 종료 코드 0, PASS |
| Player 실행 파일 Import 검사 | Direct2D/DirectWrite/파일 대화상자 DLL 의존성 없음 |
| CMake 기본 구성 생성 | 통과 |

프로젝트 검사: 한글/일본어/이모지 이름 생성, UTF-8 상대 경로 저장, 파일 재열기, 최근 목록, 프로젝트 전체 폴더 이동, 기존 디렉터리 덮어쓰기 거부, 잘못된 상대 경로 거부, 파일 교체 저장을 확인했습니다.

프로세스 검사: 한글 이름의 helper 실행 파일을 한글 작업 폴더에서 실행하고 한글/일본어/이모지/공백/빈 인자/인용부호/끝 역슬래시/&/괄호가 그대로 전달되는지 확인했습니다. helper는 창 없이 실행합니다.

런타임 검사: 한글 이름의 Scene, Mesh, PNG Texture, Vertex/Pixel Shader와 한글 이름의 중첩 HLSL include를 실제로 읽고 WARP에서 자원 생성·Shader Compile·공유 캐시를 확인했습니다. Terrain Worker나 게임 창을 시작하지 않는 제한된 자원 검사입니다.

Player 데이터 검사는 --check-project --project <한글 경로> --report <한글 경로>로 수행했습니다. 이 모드는 Window/Renderer/게임 루프를 만들지 않습니다.

## 검증 환경과 범위

- Windows SDK 10.0.26100.0, MSVC v143(앱 빌드), 테스트 스크립트의 자동 탐색 MSVC를 사용했습니다.
- 기존 PDB 도구 충돌 때문에 자동 빌드 프로세스에만 /Z7 및 /DEBUG:NONE /INCREMENTAL:NO 우회를 적용했습니다. 기본 프로젝트의 Debug 설정이나 시스템 설치는 수정하지 않았습니다.
- 실제 소스 사본: 한글 빌드 & (검증)/DXFramework_v018
- 실제 출력 폴더: 한글 출력 & (검증)/실행 파일
- 새 v0.18 폴더만 추가했으며 기존 버전의 Git 추적 파일 변경은 없습니다.
- 기존 전체 테스트를 반복 실행하거나 실행 중 DLL 교체/IPC를 검사하지 않았습니다. 이번 단계에는 해당 기능을 추가하지 않았습니다.
- CMake는 구성 생성까지 확인했습니다. 실제 앱 빌드는 Native 솔루션과 Build.ps1로 검증했습니다.
- 플레이 테스트 및 Editor/Player GUI 조작은 사용자 요청에 따라 수행하지 않았습니다.

## 사용자가 확인할 항목

1. DXEditor에서 프로젝트 생성 / 열기를 누르고 한글 상위 폴더와 이름으로 생성합니다.
2. 새 .dxproject, Assets/Shaders/Source/Build가 생성되고 Scene이 표시되는지 확인합니다.
3. Engine 패널에서 편집하고 Scene 저장 후 프로젝트 설정 저장으로 시작 Scene을 지정합니다.
4. Editor를 닫았다 열거나 프로젝트 전체를 다른 한글 폴더로 이동한 뒤 파일 열기로 다시 확인합니다.
5. 최근 목록으로 다시 열리는지 확인합니다. 이동 전의 오래된 최근 경로는 사용할 수 없으므로 파일 열기를 사용하세요.
6. Player 실행 (저장본)을 눌러 같은 Scene/자원이 별도 창에서 열리는지 확인합니다.
7. Player 종료 요청과 Editor 종료 시 Player의 정상 종료 요청을 확인합니다.
8. 기존 UI Designer/Collider 도구는 DXLegacyDemo에서 확인할 수 있습니다.

프로젝트 전환 전 저장이 필요하며, 별도 Player는 저장본을 사용합니다. Play/Stop의 편집 World 복제·복원과 본격적인 Hierarchy/Inspector 화면은 다음 단계입니다.
