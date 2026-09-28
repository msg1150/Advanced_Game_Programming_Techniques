# Disk Terrain Streaming v0.11 · UI Framework v0.3 — Scrollable Panel

## 이번 단계 목적

정상 동작을 확인한 `v0.10.2 Geomorph Debug`를 그대로 보존하고, **UI Framework 자체에 Panel Scroll 기능을 추가**한 단계입니다. Terrain/Streaming/Geomorph/Prefetch/Cache 알고리즘은 변경하지 않았습니다.

프로젝트 내부 README는 이 `README.md` **한 개만** 존재합니다.

## 추가 기능

Panel의 Widget 전체 높이가 현재 Panel 본문보다 길면 Scrollbar가 자동으로 나타납니다. 각 Panel은 독립적인 `scrollOffsetY`를 가지며 Panel을 이동하거나 다른 Panel을 열어도 서로의 Scroll 위치에 영향을 주지 않습니다.

- **Mouse Wheel**: Scroll 가능한 Panel 본문 위에서는 위/아래 Scroll
- **Wheel + UI 밖**: 기존 Camera Zoom 그대로 유지
- **Scrollbar Thumb Drag**: 좌클릭으로 잡고 위/아래 이동
- **Scrollbar Track Click**: 클릭 위치로 Thumb을 이동한 뒤 그대로 Drag 가능
- **Window Resize**: Content가 더 이상 넘치지 않으면 Scroll Offset을 0으로 복귀
- **Clip 유지**: 화면에 보이는 Content만 Render/HitTest되므로 잘린 Widget이 클릭되는 문제 방지

Terrain 설정 Panel의 높이를 억지로 키우지 않았습니다. 따라서 Widget이 늘어나도 Panel 크기는 유지하면서 아래 항목을 Scroll로 확인할 수 있습니다.

## 입력 충돌 방지

기존에는 Mouse Wheel을 항상 Camera Zoom에 전달했습니다. v0.11에서는 `UIManager::IsWheelCaptured()`를 추가했습니다.

```text
Mouse가 Scroll 가능한 Panel 위
    -> UIManager가 Wheel 소비
    -> Panel Scroll
    -> Camera Zoom 차단

Mouse가 UI 밖 또는 Scroll이 필요 없는 Panel 위
    -> UI가 Wheel을 소비하지 않음
    -> 기존 Camera Zoom
```

`CameraController::Update()`에는 마지막 선택 인자 `allowZoom=true`만 추가했습니다. 기본값이 `true`이므로 다른 호출부의 기존 동작은 유지됩니다.

## 코드 구조

```text
Source/UI/
  UIScroll.h      : Scroll 범위, Wheel 이동, Thumb 크기/위치, Drag 역변환 순수 계산
  UILayout.h      : 기존 1열/2열 배치 + 선택적 Scroll Offset/Scrollbar 여백
  UIManager.*     : Panel별 Scroll 상태, Wheel/Thumb 입력 라우팅, Scrollbar 렌더 연결
  UITheme.h       : Scrollbar 색상/폭/최소 Thumb/휠 이동량

Source/Core/Application.cpp
  UI가 Wheel을 소비한 프레임에만 Camera Zoom 차단

Source/Graphics/CameraController.*
  기존 Orbit/WASD/Zoom 구조 유지 + allowZoom 선택 인자
```

`UIScroll.h`는 Terrain, Camera, Win32 메시지를 직접 알지 않습니다. Scroll 기능의 수치 계산은 독립적으로 제거/교체할 수 있고 기존 Widget 클래스도 수정하지 않았습니다.

## UXML 조사와 다음 UI 제작 시스템 방향

Unity UI Toolkit의 현재 구조를 참고했습니다. Unity는 **UXML**로 UI 계층/구조를 정의하고, **USS**로 스타일을 분리하며, UI Builder에서 Hierarchy·Library·Viewport·Inspector를 통해 UXML/USS를 시각적으로 편집합니다. 런타임에서는 UIDocument가 UXML 기반 Visual Tree를 로드하고 코드가 동작을 연결하는 구조입니다.

이 프로젝트도 Unity 파일을 그대로 복제하는 대신 같은 **역할 분리 원칙**을 따르는 방향이 적합합니다.

```text
UI 구조 파일 (.uxml과 유사한 자체 XML)
        -> UIDocument Loader
        -> Widget Factory
        -> UI Element Tree

UI Style 파일 (.uss와 유사)
        -> Style Sheet / Theme

게임 C++
        -> Binding Registry
        -> 실제 Terrain/Game 값 및 Event 연결
```

다음 단계에서는 Editor를 먼저 만들지 않고 **Runtime UIDocument + XML Loader + Widget Factory**를 먼저 구현하는 것을 기준으로 합니다. Runtime 문서 형식이 안정된 뒤 Hierarchy / Canvas / Inspector를 가진 UI Designer를 그 위에 올리면 Editor가 저장한 파일과 게임이 읽는 파일이 동일해집니다.

> 주의: Unity의 `.uxml` 자체와 호환되는 Parser를 만드는 것이 목표는 아닙니다. UXML의 구조/스타일/로직 분리 개념을 참고하여 이 DirectX Framework에 맞는 단순한 XML UI 포맷을 설계하는 방향입니다.

## 테스트

Windows에서 확인:

1. `DXFramework.sln` → `Debug | x64` 빌드 및 실행
2. `설정 열기` 클릭
3. Panel 오른쪽에 Scrollbar가 보이는지 확인
4. Panel 본문 위에서 Wheel → UI만 Scroll되고 Camera Distance는 변하지 않는지 확인
5. Scrollbar Thumb을 Drag하여 아래쪽 설정까지 이동 가능한지 확인
6. Mouse를 Panel 밖으로 이동 후 Wheel → 기존 Camera Zoom 확인
7. Slider 숫자 직접 입력, Checkbox, Panel 제목줄 Drag, UI 테스트 Panel 등 기존 기능 회귀 확인

Portable 테스트:

```bash
clang++ -std=c++20 -Wall -Wextra -Werror -I Tests/Mocks -I Source Tests/test_ui_scroll.cpp -o test_ui_scroll
./test_ui_scroll

clang++ -std=c++20 -Wall -Wextra -Werror -I Tests/Mocks -I Source -include Tests/Mocks/compat.h Tests/test_ui_scroll_interaction.cpp Source/UI/UIWidget.cpp Source/UI/UIManager.cpp -o test_ui_scroll_interaction
./test_ui_scroll_interaction
```

기존 UI Layout/Interaction, Streaming, Prefetch/Cache, Geomorph 테스트도 그대로 유지합니다.

Windows SDK/MSVC/Direct3D11 실제 화면은 이 작업 환경에서 실행할 수 없으므로 최종 Scrollbar 렌더링과 Camera 입력 분리는 사용자 PC에서 확인해야 합니다.
