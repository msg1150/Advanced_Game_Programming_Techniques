# Disk Terrain Streaming v0.9 · Custom UI Framework v0.2

**기준:** 실행 확인을 받은 Disk Terrain Streaming v0.8.1을 별도 보존하고, 재사용 가능한 UI 구조로 정리한 완성 프로젝트입니다. 기존 Terrain/Streaming/Prefetch/Cache/Tile 파일의 알고리즘은 변경하지 않았습니다. 이 폴더와 ZIP 안의 README는 **이 `README.md` 하나뿐**입니다. 이전 버전 README 복사본이나 루트 저장소 README는 포함하지 않습니다.

## 주요 변경 사항

| 구분 | 구현 내용 |
| --- | --- |
| 공통 패널 등록 | `UIManager::CreatePanel`, `AddToPanel`, `StartSecondColumn(panelId)`로 다른 Feature가 Terrain 없이 패널 등록. 기존 `Add` / `StartSecondColumn()`은 기본 Terrain 패널용으로 유지. |
| 레이아웃 | `UILayout.h`에서 Widget의 `PreferredHeight()`를 이용해 1열/2열 배치, 패널 내용 영역 계산, 잘리는 Widget의 HitTest 제한. |
| 다중 패널 입력 | 겹친 패널은 나중에 등록한 패널이 전면. 버튼은 패널 위. UI 시작 클릭부터 Release까지 카메라 Orbit 차단, 포커스 있는 숫자 편집 중 WASD 차단. 휠은 카메라 Zoom 전용으로 유지. |
| 스타일 | `UITheme.h`에 기존 색상·폰트(맑은 고딕)·크기·간격·통계 배경 불투명도 0.68을 모음. 기존 사용자 UI 외형 및 마지막에 확정한 Terrain 표기 유지. |
| 렌더링 | `UIRenderer::PushClip` / `PopClip`으로 Panel 본문에만 그리기. 작은 창에서도 Widget이 제목줄이나 Panel 밖으로 넘쳐 그려지지 않음. |
| 위젯 | 기존 Label / Checkbox / Slider(숫자 직접 입력 포함) / Button 재사용. `UIButton`을 독립 Demo Feature에서도 실제 사용. |
| 독립성 검증 | `Source/Features/UIDemo/`는 Terrain 관련 파일을 포함하지 않고, UI 공통 API만 이용해 Checkbox·Slider·Button·숫자 입력 테스트 패널을 구성. |

### 실행

Visual Studio 2022에서 `DXFramework.sln` 열기 → **Debug | x64** → 빌드/실행. 프로그램은 이전과 동일하게 **대형 Disk Terrain 1개**만 렌더링하고 모든 설정 패널은 시작 시 닫혀 있습니다.

- 화면 오른쪽 위 `설정 열기` 버튼: 기존 Terrain 설정 패널 표시/숨김. `Load 반경`, 미니맵 배율, Predictive Prefetch 및 Cache 설정은 이전과 동일하게 실제 Terrain Getter/Setter와 연결됩니다.
- `UI 테스트` 버튼: 별도의 **UI Framework 테스트** 패널 표시/숨김. Terrain 값을 바꾸지 않으며 Checkbox, Slider, Button(클릭 횟수 표시)을 독립적으로 시험할 수 있습니다. 두 패널은 각각 독립적으로 열고 닫을 수 있습니다.
- 제목줄 좌클릭 드래그: 각 패널을 독립적으로 이동. 창 크기가 바뀌면 배치를 화면 안쪽으로 제한합니다. 패널의 본문이 화면보다 클 정도로 작은 창에서는 본문이 Clip되며, 현재 단계에 스크롤·자동 축소는 포함하지 않습니다.
- Slider 바 드래그: 기존 Step 단위 조절. 오른쪽 숫자 상자 클릭 → 입력 → `Enter` 또는 바깥 클릭으로 적용, `Esc` 또는 창 Focus 상실로 취소. 범위 밖 숫자는 Clamp되고 잘못된 문자열은 적용하지 않습니다.
- WASD 이동 / UI 바깥 좌클릭 드래그 Camera Orbit / 마우스 휠 Camera Zoom 유지. **휠은 Load 반경을 변경하지 않습니다.** Debug 통계는 이전과 동일하게 반투명 배경을 사용합니다.

**문구 기준:** 직전 사용자 확정 `TerrainSettingsPanel.cpp`의 UI 문자열을 반영했습니다. `QuadTree`, `Prefetch`, `Cache`, `Loading`, `Load`, `UnLoad` 등 사용자가 그대로 두기로 결정한 문구는 임의로 재번역하거나 수정하지 않았습니다.

### 코드 책임

```text
Source/UI/
  UIWidget.h/.cpp     : 개별 Widget의 그리기/Pointer/숫자 편집과 Callback
  UIManager.h/.cpp    : 다중 Panel 등록·Z-order·Focus·Mouse Capture·입력 라우팅
  UILayout.h          : 게임 독립 배치 계산, 본문 Clip 및 교차 판정
  UIPlacement.h       : 창 경계 제한 및 다중 상단 버튼 배치
  UITheme.h           : 공통 UI 스타일/폰트/간격/통계 배경 투명도
  UIRenderer.h/.cpp   : Direct2D/DirectWrite 백엔드, Panel Clip 스택
  UINumericValue.h   : 숫자 입력 검증 및 Clamp
  UISliderMath.h     : Slider 좌표 계산
Source/Features/TerrainUI/TerrainSettingsPanel.cpp : Terrain과 공통 Widget 연결
Source/Features/UIDemo/UIDemoPanel.h/.cpp           : Terrain과 무관한 실제 API 사용 예제
Source/Core/Application.*                         : 두 Feature만 등록, 이전 Update/Render 순서 유지
```

`UIManager`, `UILayout`, `UITheme`, `UIWidget`은 `DiskTerrainManager`를 포함하지 않습니다. Demo는 Terrain Feature를 포함하지 않습니다. 단, **UIRenderer 자체는 이 DirectX 11 프레임워크의 Direct2D/DirectWrite 백엔드**이므로 다른 그래픽 엔진에 그대로 링크하는 의미의 완전한 플랫폼 독립 UI는 아닙니다.

### 테스트

프로젝트 `Tests/Mocks`에는 **Windows SDK가 없는 환경에서 UI 로직만 빌드하는 대체 Renderer/Input**이 있으며, 실제 VS 애플리케이션 빌드에 등록되지 않습니다.

프로젝트 루트에서 LLVM `clang++` 기준:

```bash
clang++ -std=c++20 -Wall -Wextra -Werror -I Tests/Mocks -I Source Tests/test_ui_framework_layout.cpp -o test_ui_layout
./test_ui_layout
clang++ -std=c++20 -Wall -Wextra -Werror -I Tests/Mocks -I Source -include Tests/Mocks/compat.h Tests/test_ui_framework_interaction.cpp Source/UI/UIWidget.cpp Source/UI/UIManager.cpp Source/Features/UIDemo/UIDemoPanel.cpp -o test_ui_interaction
./test_ui_interaction
```

기존 `Tests/test_numeric_value.cpp`, `test_ui_placement.cpp`, `test_ui_and_streaming_policy.cpp`, `test_prefetch_cache.cpp`, `test_streaming_timing.cpp`, `test_tile_archive.cpp`도 그대로 유지합니다.

**Windows 실제 회귀 검사:** Debug x64 실행 → Terrain 설정 및 UI 테스트 두 버튼 모두 독립 동작 → 각 패널 드래그 → 기존 체크박스/슬라이더/숫자 입력/Enter/Esc → Demo Button 클릭 횟수 → 작은 창의 본문 Clip → WASD/휠/Orbit → Debug 통계 및 미니맵 → Prefetch/Cache 시간 통계 순으로 확인하세요.

Windows SDK, MSVC, DirectX GPU 화면을 사용할 수 없는 작업 환경에서는 **실제 Windows 빌드/렌더링 및 모든 입력 장치 실동작은 미검증**입니다. 모의 Renderer/Input 기반 UI 로직·정적 프로젝트 구성을 검증한 것과 구분해야 합니다. `.h/.cpp` UTF-8 BOM, HLSL UTF-8 without BOM, MSVC `/utf-8`를 유지합니다.
