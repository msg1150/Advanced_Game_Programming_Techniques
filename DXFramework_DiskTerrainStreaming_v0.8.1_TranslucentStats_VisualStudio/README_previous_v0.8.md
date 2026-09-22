# Disk Terrain Streaming v0.8 — 접이식 한글 UI Framework

## 변경 목적

Windows 환경에서 실행 확인한 `v0.7_LoadMetrics`를 보존한 새 프로젝트입니다. 256개 Tile, Disk Streaming, Prefetch/Cache, Cache Miss 및 시간 계측, 카메라 이동/휠과 기존 슬라이더 Setter/Getter는 변경하지 않습니다. Dear ImGui를 사용하지 않는 자체 Direct2D/DirectWrite UI입니다. 저장소 **루트 README는 수정하지 않습니다**.

### 실행 화면 / 조작법

- **실행 기본 상태: 설정창 접힘.** 우측 상단 `설정 열기` 버튼만 표시하여 Terrain을 가리지 않습니다.
- `설정 열기`를 클릭하면 두 열의 설정 패널이 나타납니다. 버튼이 `설정 닫기`로 바뀌고 다시 클릭하면 접힙니다. 체크·반경·캐시 설정값은 접어도 유지됩니다.
- 펼친 패널의 **제목줄(상단 43px)을 좌클릭 드래그**하면 위치를 옮길 수 있습니다. 창 밖으로 넘어가지 않도록 제한하며, 창 크기가 변경되어도 현재 위치를 가능한 범위에 맞게 보정합니다. 열기/닫기 버튼은 창 오른쪽 상단에 고정되어 언제든 누를 수 있습니다. 설정 패널은 통계 패널보다 앞에 그려집니다.
- UI 클릭/드래그에서는 Camera Orbit을 차단하고, UI 밖의 좌클릭 드래그는 기존대로 회전합니다. 숫자 입력 중에만 WASD를 차단하며, 일반 상태의 WASD 이동과 **휠 카메라 확대·축소는 그대로**입니다. 반경은 UI 슬라이더 또는 직접 숫자 입력으로만 변경합니다.
- 패널 안에서 `디버그 통계`, `쿼드트리 LOD`, `시야 밖 영역 컬링`, `타일 경계선`, `디스크 스트리밍`, `스트리밍 미니맵`, `이동 예측 프리페치`, `CPU 타일 샘플 캐시`를 체크할 수 있습니다. **F1~F6 기능 토글을 다시 추가하지 않았습니다.**
- 슬라이더 `로드 반경`, `미니맵 확대 (%)`, `프리페치 거리`, `캐시 용량 (MiB)`는 이전과 같은 범위/단위·숫자 입력(Enter 적용, Esc 취소)을 유지합니다.
- UI와 통계에 표시되는 자연어는 한글로 번역했고 QuadTree, LOD, CPU/GPU, MiB, Prefetch 등 업계에서 통용되는 약어·전문용어는 필요에 따라 남겼습니다. 글꼴은 한국어 표시를 위해 `Malgun Gothic`(맑은 고딕), DirectWrite locale은 `ko-kr`로 설정합니다. 미니맵 나침반 N/E/S/W는 방위 기호이므로 유지합니다.

## 수정/추가 소스

```text
Source/UI/UIPlacement.h             창 기준 버튼 배치와 이동 가능한 패널 위치 보정 (순수 정책)
Source/UI/UIWidget.h/.cpp           재사용 가능한 UIButton, Slider 하단 최소/최대 한글화
Source/UI/UIManager.h/.cpp          접기/펼치기, 버튼 입력 우선순위, 제목줄 Drag 및 Capture
Source/UI/UIRenderer.cpp            맑은 고딕 + ko-kr
Source/Features/TerrainUI/TerrainSettingsPanel.cpp  체크/슬라이더/성능 통계 한글 표기
Source/Core/Application.cpp         창 제목/주석 한글화 (Terrain 동작 변경 없음)
Tests/test_ui_placement.cpp         사이드 버튼·창 크기·패널 클램프 단독 테스트
```

기존 `README.md`는 `README_previous_v0.7.md`로 그대로 보존했습니다. 프로젝트의 런타임 Tile/Worker/LOD/Cache/Prefetch 로직, Shader, Assets 256개와 빌드 프로젝트 소스 등록은 그대로 유지됩니다. `UIPlacement.h`는 헤더 전용이라 `.vcxproj`/`.filters`/CMake에 **헤더 등록만** 추가했습니다.

## 테스트 절차

1. VS 2022에서 `DXFramework.sln` → `Debug | x64`로 빌드 및 실행. 처음에는 `설정 열기` 버튼만 보여야 합니다.
2. 버튼 클릭 → 패널 열림 → 체크박스와 네 슬라이더, 숫자 직접 입력이 동작하는지 확인. 다시 클릭 → 패널만 접히고 설정은 유지돼야 합니다.
3. 제목줄을 잡고 좌측/아래쪽으로 이동. 드래그 중 카메라가 돌지 않아야 하며, 창 크기를 바꿔도 버튼은 화면 오른쪽에 남아야 합니다.
4. UI 밖에서 WASD, 좌클릭 드래그, 마우스 휠을 사용해 카메라 조작을 확인. 휠만 움직일 때 로드 반경 수치는 변하면 안 됩니다.
5. `디버그 통계` 체크 후 Cache Hit/Miss, Disk/Tile Time이 한글 항목으로 출력되는지 확인. 패널을 닫아도 통계 체크 상태는 유지됩니다.
6. 미니맵 표시/확대, 프리페치 ON/OFF, 캐시 용량과 경계선 변경이 이전 v0.7과 동일하게 작동하는지 확인합니다.

### 유의점

- 기본 1280×720 화면을 기준으로 두 열을 배치합니다. 아주 작은 창에서는 위젯이 하단으로 잘릴 수 있으며, 스크롤·한 열 자동 전환은 아직 구현하지 않았습니다.
- 펼친 설정창은 지형을 가릴 수 있지만, 기본 접힘과 화면 이동 기능으로 필요할 때만 표시하도록 했습니다.
- 플랫폼 독립 위치 계산은 `Tests/test_ui_placement.cpp`로 검증할 수 있습니다. 이 제작 환경에서는 Windows SDK/DirectX/MSVC 빌드 및 실제 화면 렌더링을 수행할 수 없으므로 로컬 Visual Studio에서 실제 동작 테스트가 필요합니다.
