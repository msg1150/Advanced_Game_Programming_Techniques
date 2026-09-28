# Disk Terrain Streaming v0.12.1 · UI Framework v0.4 Build Fix

> **v0.12 MSVC 빌드 수정:** `UIWidgetFactory.cpp`에서 `std::invalid_argument`를 사용하는데 `<stdexcept>`가 누락되어 발생한 C2039/C3861 오류를 수정했습니다. UI 문서/XML/Binding 동작과 기존 Terrain 로직은 변경하지 않았습니다. `QuadTreeLOD.cpp`의 C4100 `sourceVertices`는 빌드 실패 원인이 아닌 경고이며 이번 수정 범위에서는 기존 로직 보존을 위해 그대로 둡니다.

---

# Disk Terrain Streaming v0.12 · UI Framework v0.4 — UIDocument XML

## 이번 단계 목적

실행 확인이 끝난 `v0.11 Scrollable Panel`을 보존하고, **C++ 코드를 수정하지 않고 UI 구조를 파일에서 불러올 수 있는 Runtime UIDocument 계층**을 추가한 단계입니다. Terrain / Streaming / Prefetch / Cache / QuadTree LOD / Geomorph 알고리즘은 변경하지 않았습니다.

프로젝트 전체 README는 이 `README.md` **한 개만** 존재합니다.

## 기존 UI 도구 조사 후 선택한 구조

이번 구조는 특정 엔진 포맷을 복제하지 않고 여러 UI 시스템의 공통 장점을 참고했습니다.

- **Unity UI Toolkit**: UXML이 구조, USS가 스타일, UI Builder가 Hierarchy/Library/Viewport/Inspector를 담당합니다. Runtime과 Editor가 같은 UI Asset을 사용하는 분리가 핵심입니다.
- **Unreal UMG / MVVM**: Widget Designer의 시각 구조와 Viewmodel/View Binding을 분리하여 디자이너가 표시를 바꿔도 게임 로직을 직접 건드리지 않게 합니다.
- **Qt QML**: Object Tree와 Property Binding을 선언적으로 정의하여 값 변경이 UI에 반영되는 구조를 사용합니다.
- **WPF / NoesisGUI XAML**: XML 기반 Markup이 Object/Property 구조를 선언하고 Runtime 코드와 UI 정의를 분리합니다.
- **Godot Control / Container**: UI도 Node Tree로 취급하고 Content Control과 Layout Container를 분리합니다.
- **RmlUi**: XML 문서 + CSS 계열 Style + MVC Data Binding 구조를 사용합니다.
- **Dear ImGui**: Tool 제작에는 Immediate API가 매우 편하지만, 이번 목표는 저장 가능한 Designer Asset이므로 기존 Retained Widget 시스템을 유지합니다.

따라서 이 프로젝트는 다음 방향을 채택했습니다.

```text
.ui.xml
   ↓ UIXmlParser
UIDocumentDefinition (공통 Tree / IR)
   ↓ UIWidgetFactory
기존 UIManager + UIWidget

C++ Game/Feature
   ↓ 이름 등록
UIBindingRegistry
   ↓
CheckBox / Slider / Label / Button
```

XML Parser와 Widget Factory 사이에 `UIDocumentDefinition`을 둔 이유는, 차후 Designer가 이 Tree를 직접 편집하고 `UIXmlWriter`로 저장할 수 있게 하기 위해서입니다. Designer 전용 별도 파일 형식은 만들지 않습니다.

## 새 파일

```text
Source/UI/Document/
  UIDocument.h          : UI Element Tree, Attribute, Diagnostic, Runtime ID Index
  UIBindingRegistry.h   : bool / float / text / action 이름 ↔ C++ Callback 연결
  UIUtf8.h              : UTF-8 UI Asset ↔ std::wstring 변환
  UIXmlParser.*         : 자체 UI XML Parser
  UIXmlWriter.*         : UIDocument Tree를 같은 XML 포맷으로 저장
  UIWidgetFactory.*     : XML Element → 기존 Label/CheckBox/Slider/Button 생성
  UIDocumentLoader.*    : File/String Parse + Factory Runtime 진입점

Assets/UI/
  UIDemo.ui.xml         : 실제 Runtime에서 불러오는 문서 예제
```

외부 UI/XML 라이브러리를 프로젝트 의존성으로 추가하지 않았습니다. `UIXmlParser`는 이 UI 문서에 필요한 XML 선언, Comment, Attribute, Self-closing/Nested Element, 기본 Entity만 담당하는 작은 전용 Parser입니다. 범용 Browser/XML 엔진을 목표로 하지 않습니다.

## 현재 UIDocument v1 문법

```xml
<?xml version="1.0" encoding="utf-8"?>
<UIDocument version="1">
  <Panel id="Settings" title="설정" openLabel="설정 열기" closeLabel="설정 닫기"
         width="500" height="425">
    <Column>
      <Label id="Title" text="Graphics" />
      <CheckBox id="LOD" text="QuadTree LOD" binding="Terrain.LOD" />
      <Slider id="Radius" text="Load 반경" min="28" max="160" step="1"
              binding="Terrain.LoadRadius" />
      <Button id="Reset" text="초기화" action="Terrain.Reset" />
      <Label id="Status" textBinding="Terrain.StatusText" />
    </Column>
  </Panel>
</UIDocument>
```

현재 지원 Widget은 기존 Framework에 이미 있던 `Label / CheckBox / Slider / Button`입니다. Panel은 최대 2열 `Column` 배치를 지원하며 기존 Scrollbar/Clip/입력 시스템을 그대로 사용합니다.

Interactive Widget에 Binding이 없으면 문서 안의 `value`를 사용하는 **Local Preview State**로도 생성할 수 있습니다. 따라서 향후 Designer Canvas에서 게임 Binding 없이도 Checkbox/Slider를 미리 조작할 수 있는 기반이 있습니다.

## Binding 예제

XML은 C++ 객체를 직접 참조하지 않고 문자열만 저장합니다.

```xml
<CheckBox text="테스트 Checkbox" binding="Demo.Enabled" />
<Button text="테스트 Button" action="Demo.Click" />
```

C++ Feature에서 실제 동작을 등록합니다.

```cpp
bindings.RegisterBool("Demo.Enabled",
    [this]{ return checkbox_; },
    [this](bool value){ checkbox_=value; });

bindings.RegisterAction("Demo.Click",
    [this]{ ++clicks_; });
```

`UIDemoPanel`은 v0.12부터 `Assets/UI/UIDemo.ui.xml`에서 실제 Panel을 생성합니다. XML 파일이 없거나 Syntax Error가 생기면 기존 Code-driven UI를 Fallback으로 생성하여 Terrain 실행 자체가 실패하지 않도록 했습니다.

## 기존 코드와의 독립성

기존 `UIManager::Add`, `AddToPanel`, `CreatePanel`, `StartSecondColumn` API는 삭제하거나 변경하지 않았습니다. `TerrainSettingsPanel.cpp`도 아직 Code-driven 방식 그대로입니다.

즉 이 단계는 **새 Data-driven 경로를 옆에 추가**한 것이고, 문제가 생기면 `Source/UI/Document/`와 `Assets/UI/` 연결만 제거해 기존 UI Framework로 돌아갈 수 있습니다.

다음 단계에서 Terrain 설정 UI를 XML/Binding으로 이전하거나, 바로 Designer의 `Hierarchy / Canvas / Inspector / Widget Palette` 기반을 만들 수 있습니다. Designer는 이번에 추가한 `UIDocumentDefinition`과 `UIXmlWriter`를 그대로 사용합니다.

## 테스트

Portable Test:

```bash
clang++ -std=c++20 -Wall -Wextra -Werror -I Source   Tests/test_ui_document_xml.cpp Source/UI/Document/UIXmlParser.cpp Source/UI/Document/UIXmlWriter.cpp   -o test_ui_document_xml

clang++ -std=c++20 -Wall -Wextra -Werror -I Source   Tests/test_ui_binding_registry.cpp -o test_ui_binding_registry

clang++ -std=c++20 -Wall -Wextra -Werror -I Tests/Mocks -I Source -include Tests/Mocks/compat.h   Tests/test_ui_document_factory.cpp Source/UI/UIWidget.cpp Source/UI/UIManager.cpp   Source/UI/Document/UIXmlParser.cpp Source/UI/Document/UIWidgetFactory.cpp   Source/UI/Document/UIDocumentLoader.cpp -o test_ui_document_factory
```

기존 Scroll/Layout/Input/Streaming/Prefetch/Cache/Geomorph 테스트도 그대로 유지합니다.

Windows 확인 순서:

1. `DXFramework.sln` → `Debug | x64` 빌드/실행
2. 우측 상단 `UI XML` 버튼 확인
3. 열면 `UIDocument XML 테스트` Panel이 표시되는지 확인
4. Checkbox/Slider/Button이 기존과 동일하게 동작하는지 확인
5. Button 클릭 횟수 Label이 C++ Text Binding을 따라 변하는지 확인
6. `Demo 값 초기화` Action 확인
7. 기존 `설정 열기`, Scrollbar, Camera Wheel/Orbit, Terrain/Geomorph 기능 회귀 확인
8. 테스트용으로 `Assets/UI/UIDemo.ui.xml`의 문구나 Panel 크기를 바꾼 뒤 재빌드/실행하여 C++ 변경 없이 UI Asset 변경이 반영되는지 확인

이 작업 환경에서는 Windows SDK/MSVC/Direct3D11 실제 실행 화면을 검증할 수 없으므로 최종 Runtime 확인은 사용자 PC에서 진행해야 합니다.
