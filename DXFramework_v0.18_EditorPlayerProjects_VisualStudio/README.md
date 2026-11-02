# DXFramework v0.18 — Runtime / Editor / Player / Projects

v0.17(a9f9693)을 별도 폴더로 복제한 첫 엔진·에디터 분리 단계입니다. 이전 버전은 수정하지 않습니다. 교수님 예제에서는 실행 구조와 프로젝트 관리 흐름을 참고했으며, DLL/IPC/ImGui 코드를 가져오지는 않았습니다.

## 실행 구조

- **DXRuntime.lib**: Scene/Component/직렬화/자원/충돌/지형 및 DirectX·입력·창 백엔드. Editor UI를 참조하지 않습니다.
- **DXEditor.exe**: 프로젝트 생성·열기·최근 목록, 기존 자체 UI 기반 Engine 편집 패널, Player 실행.
- **DXPlayer.exe**: 같은 Runtime과 프로젝트를 읽어 독립 실행. Direct2D/DirectWrite/파일 대화상자에 의존하지 않습니다.
- **DXLegacyDemo.vcxproj**: v0.17의 Terrain 설정·UI Designer·Collider 도구 작업 화면. 필요할 때 별도로 빌드합니다.

기본 DXFramework.sln에는 Runtime/Editor/Player 세 대상이 있습니다. 실제 Scene 실행은 Engine/Runtime/RuntimeSession에 모았고 Editor와 Player가 같은 코드를 사용합니다. RuntimeSession의 서비스·ColliderWorld·Scene 수명은 인스턴스별로 분리합니다.

## 빌드

Visual Studio 2022에서 DXFramework.sln을 열고 **Debug | x64**, 시작 프로젝트 **DXEditor**를 선택합니다.

PowerShell 7에서는 다음 스크립트도 사용할 수 있습니다.

```powershell
./Tools/Build.ps1
# 이 검증 환경처럼 PDB 도구 버전 충돌이 있는 경우에만:
./Tools/Build.ps1 -NoPdbWorkaround
# 기존 작업 화면:
./Tools/Build.ps1 -Target DXLegacyDemo.vcxproj -NoPdbWorkaround
```

기본 출력은 bin/x64/Debug입니다. 실행 파일 옆 Assets, Shaders, Starter.dxproject를 함께 유지하세요.

CMake도 Runtime/Editor/Player 대상을 제공합니다. 소스 목록은 Tools/TargetSources.json을 공유합니다.

```powershell
cmake -S . -B C:/temp/dx18 -G "Visual Studio 17 2022" -A x64
cmake --build C:/temp/dx18 --config Debug --parallel 1
```

Native 프로젝트에 소스를 추가할 때에는 해당 vcxproj도 함께 갱신하세요. CMake는 TargetSources.json의 목록을 읽습니다.

## 프로젝트 만들기와 열기

1. DXEditor를 실행합니다. 처음에는 배포 폴더의 Starter.dxproject를 엽니다.
2. **프로젝트 생성 / 열기**를 누릅니다.
3. 상위 폴더와 프로젝트 이름을 입력하고 생성합니다. **한글·공백·일본어·이모지 경로를 제한하지 않습니다.**
4. 템플릿의 Assets/Shaders와 시작 Scene을 복사하고 Source/Build 디렉터리 및 .dxproject를 만듭니다. 기존 폴더는 덮어쓰지 않습니다.
5. 프로젝트 파일 열기 또는 최근 목록 더블클릭으로 다시 엽니다. 단순히 여는 동작으로 프로젝트 파일을 수정하지 않습니다.
6. Engine 패널에서 기존처럼 객체/Component/Transform을 편집하고 Scene을 저장합니다.
7. **프로젝트 설정 저장**은 현재 Scene 파일을 시작 Scene으로 등록합니다. Scene 파일은 프로젝트 폴더 내부에 있어야 합니다.
8. **Player 실행 (저장본)**은 디스크에 저장된 시작 Scene을 별도 Player로 실행합니다. 종료 요청은 WM_CLOSE를 보내 정상 정리를 요청합니다.

프로젝트 전환 전에는 현재 Scene을 저장하세요. Player는 저장하지 않은 편집 상태를 전달받지 않습니다. 편집 World 복제, Play/Stop 원상 복원, 독립 Scene/Game View는 후속 단계입니다. 현재 Engine 패널의 Play/Pause는 v0.17 동작을 유지합니다.

## 프로젝트 파일

DXPROJECT version 1, UTF-8 텍스트입니다. 기본 구성 예시는 Starter.dxproject에 있습니다.

```text
DXPROJECT 1
"name" "한글 프로젝트"
"assetRoot" "."
"startupScene" "Assets/Scenes/Runtime.engine.scene"
"terrain" "Assets/TerrainTiles"
"content.0" "Assets"
"content.1" "Shaders"
```

프로젝트 경로는 파일 위치 기준 상대 경로입니다. 프로젝트 전체를 다른 한글 폴더로 이동해도 다시 열 수 있습니다. terrain을 빈 문자열로 지정하면 해당 프로젝트는 지형 없이 실행합니다. Terrain 모듈의 기본 Shader/Texture 배치는 기존 Assets/Textures/Terrain 및 Shaders/TriplanarTerrain 규칙을 사용합니다.

최근 목록은 Windows LocalAppData/DXFrameworkEditor에 UTF-8로 저장하며 프로젝트 안에 개인 머신의 SDK 경로를 기록하지 않습니다. 생성 실패 시 원본과 기존 프로젝트는 건드리지 않습니다. 생성 도중 실패한 .creating.* 임시 폴더는 데이터 복구를 위해 남을 수 있습니다.

## 한글 경로 처리

- 파일 경로는 filesystem::path, 문서 문자열은 UTF-8, Win32 API는 UTF-16 W 함수를 사용합니다.
- 경로를 string()이나 ANSI CreateProcess로 넘기지 않습니다. Player는 wWinMain/CommandLineToArgvW로 인자를 읽습니다.
- 실행 인자는 Windows 인용 규칙에 따라 처리하고 cmd /c를 거치지 않습니다. 공백·괄호·&가 명령으로 해석되지 않습니다.
- HLSL은 경로로 직접 읽고 UTF-8 include 파일명을 처리하는 ID3DInclude를 사용합니다. 한글 이름의 중첩 include도 처리합니다.
- 파일 교체 저장은 같은 폴더의 임시 파일을 완전히 쓴 뒤 교체합니다. 기존 MAX_PATH 크기의 GetTempFileName 버퍼를 제거했습니다.
- 앱 Manifest에 UTF-8 코드 페이지와 longPathAware를 설정했습니다. 운영체제·파일시스템·외부 빌드 도구의 고유 길이 제한까지 무제한으로 보장하는 의미는 아닙니다.

## 선택 모듈

FeatureOptions.props와 CMake의 기존 EngineGraphics/EngineCollision/EngineGameplay/TerrainQuery 옵션을 유지합니다. 새 EnableWorldTerrain / DX_ENABLE_WORLD_TERRAIN 옵션으로 프로젝트 지형을 제외할 수 있습니다. Scene/Project/RuntimeSession은 새 실행 파일의 공통 기반입니다. 지형을 제외한 새 Editor/Player는 동작할 수 있으며, 기존 LegacyDemo는 지형을 필요로 합니다.

이 단계는 정적 Runtime 공유와 프로젝트 관리입니다. DLL Hot Reload, 프로세스 간 실시간 Scene 동기화, 메타데이터 Inspector, Undo/Redo, Gizmo, GUID Asset Database, Prefab, 패키징은 아직 다음 단계입니다.

## 검증

```powershell
./Tools/Test.ps1 -Filter '*unicode*'
```

프로젝트 생성·저장·이동 후 재열기, Unicode 실행 파일/인자/CWD, 한글 Mesh·Texture·Shader 및 중첩 include를 자동 검증합니다. Runtime 로딩 검사는 창 없는 WARP를 사용합니다. Player의 --check-project는 창과 게임 루프를 만들지 않는 데이터 검사 모드입니다. 플레이 테스트는 사용자가 수행합니다.

작업 및 검증 기록: Docs/IMPLEMENTATION.md, Docs/VERIFICATION.md.
