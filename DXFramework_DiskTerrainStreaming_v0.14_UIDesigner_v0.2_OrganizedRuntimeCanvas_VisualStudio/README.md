# Disk Terrain Streaming v0.14 · UI Designer v0.2 — Organized Runtime Canvas

## 이번 단계 목표

`v0.13.1`의 별도 Win32 UI Designer 창 구조를 유지하면서, **Editor Shell과 Canvas Preview의 책임을 분리**했습니다. Designer의 Palette / Hierarchy / Inspector / File Dialog는 WinAPI를 그대로 사용하고, 중앙 Canvas는 더 이상 별도 GDI 모양을 흉내 내지 않고 **게임 Runtime과 같은 `UIRenderer + UIWidget + UILayout + UITheme`으로 실제 Preview를 그립니다.**

동시에 v0.12~v0.13에서 추가된 UI Document / Designer 코드가 한 폴더에 몰리지 않도록 역할별 실제 디렉터리와 Visual Studio Filter를 정리했습니다. Terrain / Streaming / Prefetch / Cache / Geomorph 로직은 변경하지 않았습니다.

이 프로젝트와 ZIP에는 **`README.md`가 정확히 1개만** 있습니다.

## Designer 구조

```text
메인 DirectX 창
  └─ UI Designer 버튼
       └─ 별도 Top-Level Win32 Tool Window
            │
            ├─ WinAPI Editor Shell
            │    ├─ Toolbar / File Dialog
            │    ├─ Widget Palette
            │    ├─ Hierarchy
            │    └─ Inspector
            │
            └─ Custom UI Canvas
                 ├─ UIRenderer (Direct2D/DirectWrite)
                 ├─ 실제 UILabel / UIButton / UICheckBox / UISlider
                 ├─ 실제 UILayout
                 └─ 실제 UITheme
                       ↓
                 UIDocumentDefinition
                       ↓
                    *.ui.xml
```

Canvas의 Child Window 생성과 Mouse Message 자체는 WinAPI가 담당하지만, **Canvas 내부 UI Preview 렌더링은 Custom UI Framework가 담당**합니다. 따라서 Runtime Widget 높이, 색상, Slider/CheckBox/Button 외형을 Designer에서 별도로 복제할 필요가 줄어듭니다.

## 새 폴더 구조

```text
Source/UI/Document/
  Model/
    UIDocument.h
    UIBindingRegistry.h

  Serialization/
    UIUtf8.h
    UIXmlParser.h/.cpp
    UIXmlWriter.h/.cpp

  Runtime/
    UIDocumentLoader.h/.cpp
    UIWidgetFactory.h/.cpp

Source/Tools/UIDesigner/
  Core/
    UIDesignerModel.h/.cpp

  Canvas/
    UIDesignerCanvas.h/.cpp

  Window/
    UIDesignerWindow.h/.cpp
```

### 각 폴더 책임

- `Document/Model`: XML/WinAPI/Renderer를 모르는 UI 문서 Tree와 Binding 이름 연결 구조.
- `Document/Serialization`: UTF-8, XML Parse/Write만 담당.
- `Document/Runtime`: 문서 Tree를 실제 `UIManager` / `UIWidget`으로 변환.
- `UIDesigner/Core`: 문서 편집 Model. Windows API 비의존이라 Portable Test 가능.
- `UIDesigner/Canvas`: Runtime Custom UI를 이용한 Preview와 Canvas HitTest.
- `UIDesigner/Window`: WinAPI Tool Window, Palette / Hierarchy / Inspector / 파일 작업만 담당.

`DXFramework.vcxproj.filters`도 실제 폴더 경로와 동일하게 다시 구성했으므로 Visual Studio Solution Explorer에서도 위 구조대로 보입니다.

## UIRenderer 확장

기존 게임 UI 경로는 그대로입니다.

```cpp
uiRenderer.Initialize(renderer.GetSwapChain());
uiRenderer.Begin(d3dContext);
```

Designer Canvas용으로 **추가 API만** 제공합니다.

```cpp
canvasRenderer.InitializeForWindow(canvasHwnd);
canvasRenderer.BeginWindow();
```

별도 Win32 `HWND`에 `ID2D1HwndRenderTarget`을 만들기 위한 경로이며, 기존 SwapChain용 API를 제거하거나 동작을 변경하지 않습니다. Canvas Resize 시에는 `ResizeWindowTarget()`만 호출합니다.

## Canvas Preview 범위

현재 XML Runtime이 지원하는 구조와 동일하게 `Panel`, 최대 2열 기준 `Column`, `Label`, `Button`, `CheckBox`, `Slider`를 Preview합니다. Widget은 실제 Runtime 클래스를 생성하여 Render하므로 기존 v0.13의 GDI 도형 Preview보다 실제 게임 화면과 일치하는 범위가 넓습니다.

아직 Absolute X/Y, Anchor, Resize Handle은 문서/Runtime에 정의되지 않았기 때문에 이번 단계에서도 임의 좌표 배치를 추가하지 않았습니다. 다음 Designer 단계에서 Layout Asset 정의 자체를 확장한 뒤 Drag/Resize를 연결하는 것이 안전합니다.

## 테스트 방법

1. Visual Studio 2022에서 `DXFramework.sln` → `Debug | x64` Build/실행.
2. 메인 `UI Designer` 버튼 → 별도 Designer Window가 열리는지 확인.
3. 중앙 Canvas의 Panel / CheckBox / Slider / Button 모양이 **메인 Custom UI와 같은 스타일**인지 확인.
4. Canvas Widget 클릭 → Hierarchy와 Inspector 선택이 동기화되는지 확인.
5. Palette에서 Widget 추가 → Canvas가 바로 갱신되는지 확인.
6. Inspector Text / Slider Min·Max·Step / Panel Size 수정 → 적용 후 Canvas 반영 확인.
7. Save As → 다시 Open → 문서 구조와 속성 유지 확인.
8. Designer 창 Resize → Canvas가 따라 Resize되고 Preview가 유지되는지 확인.
9. Designer X 버튼으로 닫아도 메인 Terrain 창이 종료되지 않는지 확인.
10. 기존 Terrain 설정 / Scroll / Camera / Streaming / Prefetch / Cache / Geomorph도 회귀 확인.

## 제거 가능성

Designer 전체를 제거할 때는 `Source/Tools/UIDesigner/`와 `Application`의 Designer 멤버/버튼 연결만 제거하면 됩니다. `UI/Document` Runtime은 독립적으로 계속 사용할 수 있습니다.

Canvas 기능만 제거하려면 `Source/Tools/UIDesigner/Canvas/`를 제거하고 `UIDesignerWindow`에서 다른 Preview를 연결하면 됩니다. Terrain / Streaming / Geomorph에는 영향이 없습니다.

## 검증 범위

Portable 환경에서 `UIDesignerModel`, XML Parser/Writer, Binding, Document Factory, 기존 UI 정책 테스트를 다시 수행합니다. 프로젝트/Filters XML과 256개 Terrain Tile, ZIP 무결성도 검사합니다.

이 작업 환경에는 Windows SDK/MSVC/실제 D3D11/Win32 GUI 실행 환경이 없으므로 **새 HWND Direct2D Canvas의 실제 Windows Build와 화면 상호작용은 사용자 PC의 `Debug | x64` 테스트가 최종 기준**입니다.
