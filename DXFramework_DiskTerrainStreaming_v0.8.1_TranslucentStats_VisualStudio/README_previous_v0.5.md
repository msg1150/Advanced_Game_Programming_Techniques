# Large Disk Terrain — Custom UI Framework v0.3 · 슬라이더 숫자 직접 입력

이 프로젝트는 정상 실행을 확인한 v0.4 MiniMap Zoom에 숫자 편집 기능만 확장한 **새 단계의 완성 프로젝트**이다.
기존 다른 버전/저장소 루트 README를 변경하지 않았다. Dear ImGui 사용 없음.

## 조작 방법

- 우측 `Terrain Settings` → `Load Radius` 또는 `MiniMap Zoom (%)`의 **숫자가 적힌 작은 상자 클릭**.
- 숫자를 입력하고 `Enter` → 실제 설정에 적용. 상자 밖 클릭도 적용. `Esc` → 취소.
- 클릭 직후 최초 입력은 기존 숫자를 모두 교체. `Backspace` 삭제. 숫자/소수점/맨 앞 +/- 기호만 허용.
- 잘못된 입력(`-`, `.` 등)은 폐기. 범위 밖 입력은 Load 28~160, MiniMap Zoom 50~400으로 제한.
- 숫자 직접 입력은 슬라이더 드래그의 `step`(Load 1, MiniMap 25)으로 **강제 반올림하지 않음**. 예: MiniMap 137 직접 입력 허용. 슬라이더를 나중에 드래그하면 기존 25단위로 조정.
- 슬라이더 바 드래그는 기존 동작 유지. UI 숫자 편집 중에는 WASD 카메라 이동 금지. UI 좌클릭으로 Orbit 금지. 휠은 기존처럼 카메라 Zoom 전용.
- 기존 여섯 체크박스, 통계, 회전형 MiniMap, Streaming/LOD/Worker/Tile 파일 로직 변경 없음.

## 변경 코드

`Source/UI/UINumericValue.h` : 엄격한 문자열 숫자 파싱 및 min/max 제한 (플랫폼 독립).

`Source/UI/UIWidget.h/.cpp` : 공통 Widget의 선택적 텍스트 입력 계약과 `UISlider` 우측 숫자 입력칸, Enter/Esc 처리.

`Source/UI/UIManager.h/.cpp` : 텍스트 입력 Focus, 문자 라우팅, 바깥 클릭 Commit, UI Mouse Capture, Keyboard Capture.

`Source/Input/Input.h/.cpp` : Win32 `WM_CHAR` 프레임별 입력 버퍼와 창 Focus 상실 이벤트.

`Source/Graphics/CameraController.h/.cpp`, `Source/Core/Application.cpp` : Focus 중 WASD 이동만 차단. 휠/기존 카메라 계산 변경 없음.

CMakeLists.txt / Visual Studio vcxproj / filters : 새 헤더 등록.

## 빠른 테스트

1. VS 2022에서 `DXFramework.sln` → `Debug | x64` 빌드 후 실행.
2. Load Radius 숫자 클릭 → `140` 입력 → Enter → 반경과 미니맵 원 변화를 확인.
3. `999` Enter → 최대 `160`으로 제한되는지, `20` Enter → 최소 `28`인지 확인.
4. MiniMap Zoom 숫자 클릭 → `137` Enter → 미니맵 격자만 변하고 로딩 범위는 유지되는지 확인.
5. 숫자 입력 중 W/A/S/D 및 마우스 휠 테스트: 이동은 막히고 휠 카메라 Zoom만 작동해야 한다.
6. Esc 및 창 Alt+Tab 시 편집이 취소되는지 확인.

## 검증 범위

`Tests/test_numeric_value.cpp`는 플랫폼 독립 숫자 검증 테스트. 이 작업 환경에서는 Windows DirectX/MSVC 전체 프로젝트 실제 빌드 및 UI 화면 테스트가 불가능하므로 로컬 VS 실행 결과를 확인해야 한다.

---

## 이전 v0.4 설명 (보존)

# Large Disk Terrain — Custom UI Framework v0.2 · MiniMap Grid Zoom

## 목적 및 기준 버전

Windows/DirectX 11 자체 제작 프레임워크의 새 UI 단계. 실행을 확인한 `Disk-based Terrain Streaming v0.2`는 별도 보존하며 이 프로젝트에서는 **기존 단축키 F1~F6 기능을 UI 체크박스로 대체**한다. Dear ImGui, RmlUi 등의 외부 UI 프레임워크는 사용하지 않는다. 기존 대형 Terrain 1개 / Disk Tile Worker / QuadTree LOD / Triplanar / 방향 회전형 미니맵은 유지한다.

## 실행 및 조작

Visual Studio 2022 `DXFramework.sln` 열기 → `Debug | x64` → Build → 실행. 프로그램 우측 상단에 항상 `Terrain Settings` 패널이 보인다. UI는 별도 실행 파일이 아니라 게임 창 BackBuffer에 직접 출력된다.

| 조작 | 기능 |
| --- | --- |
| WASD | 이전과 동일한 카메라 타겟 이동 |
| 마우스 좌클릭 드래그 (UI 밖) | 기존 Orbit 카메라 회전 |
| 마우스 휠 (UI 위 포함) | **카메라 확대/축소 전용**. Streaming 반경 불변 |
| 설정 패널 `Frustum Culling` | 예전 F1 토글 |
| `QuadTree LOD` | 예전 F2 토글 |
| `Debug Statistics` | 예전 F3 토글: 좌측 통계 패널 표시 |
| `Tile Borders` | 예전 F4 토글 |
| `Disk Streaming` | 예전 F5 토글; OFF = 전체 256개 Tile 점진적 요청/유지 |
| `Streaming MiniMap` | 예전 F6 토글: 우하단 회전형 미니맵 |
| `Load Radius` 슬라이더 | **28~160** 월드 단위, 초기값 56. 실제 Tile 로딩 범위 설정 |
| `MiniMap Zoom (%)` 슬라이더 | **50~400%**, 기본 100%. 미니맵 **내부 격자·원만** 확대/축소, 패널 크기 및 실제 로딩 범위 불변 |

기존 F1~F6 키의 전환 코드는 `Application.cpp`에서 제거되었다. 각 체크박스는 실제 `DiskTerrainManager` 상태 Getter/Setter에 직접 연결되어 있으므로 표시와 실제 동작이 서로 분리되지 않는다.

### 반경과 히스테리시스

`Load Radius`는 슬라이더로만 바뀌며 휠 Zoom과 완전히 분리되었다. `Unload Radius = Load Radius + 28`, 최대 `Load 160 / Unload 188`로 안전하게 제한한다. 기존 **Camera Target ~ Tile AABB의 XZ 최단 거리** 판정과 비동기 로딩은 그대로다. 미니맵 초록/주황 원도 이 동일한 반경을 표시한다. Streaming OFF 상태에서는 전체 타일을 유지하므로 원형 로딩 제한이 적용되지 않는다. 여기서 조절하는 값은 Tile **로딩/유지 범위**이며 FOV나 Frustum 자체를 자르는 렌더링 거리 설정은 아니다.

### UI 입력 충돌 방지

UI 위에서 좌클릭을 시작하면 버튼을 놓을 때까지 UI가 포인터를 소유한다. 이 동안 Camera Orbit은 차단되지만 WASD/휠은 기존 Input/CameraController 경로로 처리한다. 슬라이더는 밖까지 드래그해도 범위 밖 값이 들어가지 않도록 clamp한다. Info 패널은 표시 전용이고 설정 패널은 우측 상단, 미니맵은 우측 하단 배치된다. 체크박스 높이를 살짝 줄여 슬라이더 두 개를 넣고, 기본 창 1280×720에서도 미니맵과 겹치지 않게 했다. 아주 작은 창으로 줄이면 UI 배치가 제한될 수 있으며 반응형 레이아웃/스크롤은 후속 작업이다.

## 코드 구성

```text
Source/UI/
  UIRenderer.h/.cpp     : 공통 Direct2D / DirectWrite 도형·텍스트 드로잉
  UIWidget.h/.cpp       : UIWidget 부모, UIPanel, UILabel, UICheckBox, UISlider
  UIManager.h/.cpp      : Widget 등록, 레이아웃, Mouse Capture / HitTest
  UISliderMath.h        : 순수 슬라이더 좌표 계산
Source/Features/TerrainUI/
  TerrainSettingsPanel.h/.cpp : UI 체크박스/슬라이더 ↔ Terrain API 바인딩
Source/Features/DiskTerrainStreaming/
  StreamingRadiusPolicy.h : Camera Zoom 의존성 없는 Radius clamp/Hysteresis
  MiniMapZoomPolicy.h    : 미니맵 전용 50~400% 확대 배율 및 표시용 World Radius 계산
  StreamingRangeDebugRenderer.h/.cpp : 미니맵 Grid/원 배율과 지도 내부 Scissor Clip
```

공통 UI 모듈은 DiskTerrainManager를 `#include`하지 않는다. `TerrainSettingsPanel`만 UI와 실제 Terrain 기능을 연결한다. UI 렌더러는 기존 `DebugTextRenderer`와 중복 생성하지 않고, Terrain + Direct3D 미니맵을 그린 **후** BackBuffer를 D2D로 그린다. Window `WM_SIZE`에서는 UI RenderTarget 참조 해제 → SwapChain Resize → UI Target 재생성 순서다. Input에는 Mouse Client Pixel 좌표 Getter만 추가하고 CameraController에는 Orbit만 막는 선택적 파라미터를 추가했다. 키보드 이동·휠 로직은 바꾸지 않았다.

기존 TileArchive/Worker/TileGeometryBuilder/QuadTreeLOD/TriplanarMaterial과 파일 256개는 변경하지 않았다. 미니맵 Renderer에는 배율 Setter와 **지도 부분에만 적용되는 Scissor Clip**을 추가하여 400% 확대해도 격자가 범례/패널까지 튀어나오지 않게 했다. 프레임워크 이전 프로젝트 / 상위 저장소 README를 변경하지 않았다.

## 필수 테스트

1. 처음 실행하면 우측 상단 패널과 우측 하단 미니맵이 출력되는지 확인.
2. 휠 Zoom을 최소/최대까지 움직여도 `Load Radius`와 `MiniMap Zoom (%)`이 변하지 않는지 확인. 미니맵에서 원·격자 크기도 그대로여야 한다.
3. `MiniMap Zoom (%)`을 **50 → 100 → 200 → 400**으로 바꿔 **패널 크기는 그대로**이고 Tile 격자와 초록/주황 원만 확대되는지 확인. 400%에서는 미니맵 내부에만 격자가 보여야 한다.
4. Load Radius 슬라이더를 28 → 160으로 드래그할 때 `Desired / Loaded` 수와 미니맵 반경이 변하는지 확인.
5. 슬라이더를 누른 채 마우스를 화면 바깥까지 움직여도 카메라가 회전하지 않고 반경이 범위 안인지 확인.
6. 여섯 개 체크박스를 각각 껐다 켜고 원래 F1~F6 기능과 동일한지 확인.
7. `Debug Statistics`를 켜고 GPU Buffers / Disk Reads 등을 확인. Streaming OFF 시 전체 Tile을 읽을 수 있으므로 확인 후 ON으로 복귀.
8. Win32 창 크기를 바꿔 UI BackBuffer 재생성이 정상인지 확인.

`Tests/test_ui_and_streaming_policy.cpp`는 Windows SDK 없이 `clang++ -std=c++20 -I Source ...`로 반경/슬라이더의 순수 계산 테스트가 가능하다. Windows MSVC 및 GPU 런타임 빌드는 이 환경에서 검증할 수 없으므로 **Debug x64 실제 빌드와 클릭/렌더링 테스트는 아직 미검증**이다. `.cpp/.h`는 UTF-8 BOM, `.hlsl`은 기존 인코딩 유지, MSVC `/utf-8` 유지.

## MiniMap Zoom 내부 동작 및 독립성

이전 v0.3의 미니맵 배율은 `StreamingRangeDebugRenderer.cpp`의 `viewRadius = max(85, UnloadRadius * 1.12)` 고정식이었다. v0.4는 이를 `viewRadius = max(85, UnloadRadius * 1.12) / (MiniMapZoomPercent / 100)`으로 바꾼다. 따라서 **100%는 이전과 동일**, 200%면 격자 한 칸이 2배 커지고 지도에 담기는 실제 월드 면적은 가로·세로 각각 절반이 된다. 50%면 그 반대다. 로딩용 `activeRadii_` 또는 카메라 `GetDistance()` 값에는 관여하지 않는다. 체크박스 `Streaming MiniMap`이 OFF여도 슬라이더 값은 유지한다.

새 프로젝트 버전만 변경했고 이전 v0.3, 상위 저장소 README는 수정하지 않았다. Windows/MSVC + 실제 GPU 동작은 사용자 PC에서 검증해야 한다.
