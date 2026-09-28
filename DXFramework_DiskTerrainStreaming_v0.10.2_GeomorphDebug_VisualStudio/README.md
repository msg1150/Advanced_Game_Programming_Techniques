# Disk Terrain Streaming v0.10.2 · QuadTree LOD Geomorph Debug Visualization

## 이번 단계 목적

정상 동작을 확인한 `v0.10.1 Async Geomorph`를 그대로 보존하고, **Geomorphing이 실제로 어느 전이 구간에 있는지 화면에서 확인하는 Debug Visualization만 추가**한 마무리 단계입니다. 기존 QuadTree LOD 선택, Geomorph Factor 계산, 비동기 Geomorph Worker, Disk Streaming, Prefetch, Cache, UI Framework 로직은 변경하지 않았습니다.

프로젝트 내부 README는 이 `README.md` **한 개만** 존재합니다.

## 사용 방법

Terrain 설정 패널에서 다음 두 항목을 켭니다.

- `QuadTree Geomorphing` : 실제 Geomorphing 활성화
- `Geomorph Debug 색상` : 전이 진행률을 색상으로 표시

Debug 색상은 아래 의미를 가집니다.

| 색상 | 진행률 | 의미 |
| --- | ---: | --- |
| Red | 0% | Child가 처음 선택되어 Parent LOD 표면 모양에서 전이를 시작한 상태 |
| Yellow | 50% | Parent 표면과 Child 원본 표면 사이를 보간하는 중간 상태 |
| Green | 100% | Child LOD의 원래 높이/Normal에 도달한 상태 |

내부 `MorphFactor`는 `1 = Parent`, `0 = Child` 방향이지만 Debug 화면에서는 사람이 이해하기 쉽도록 `MorphProgress = 1 - MorphFactor`로 변환해 **Red → Yellow → Green** 순서로 표시합니다.

Root Node처럼 Parent가 없어 Geomorph 대상이 아닌 Range와 Geomorphing을 사용하지 않는 Range는 기존 Triplanar Texture로 렌더링됩니다. Debug 토글은 색상 표시만 바꾸며 LOD 선택이나 지형 위치를 변경하지 않습니다.

## 구현 구조

`Shaders/TriplanarTerrain/TriplanarGeomorphVS.hlsl`에 `MorphProgress : TEXCOORD2` 출력만 추가했습니다. 기존 `TriplanarPS.hlsl`은 수정하지 않았습니다.

새 `TriplanarGeomorphDebugPS.hlsl`은 Morph 진행률을 Red → Yellow → Green Gradient로 출력합니다. `QuadTreeLODGeomorphRenderer`는 일반 Geomorph Shader와 Debug Shader를 분리해서 관리하고, Debug 토글이 켜졌을 때만 Debug PS를 사용합니다.

Debug Shader는 **선택 기능**으로 초기화합니다. Debug Shader 파일이 없거나 컴파일에 실패해도 일반 Geomorph Shader가 정상 초기화됐다면 Geomorphing 자체는 계속 동작하고 일반 Triplanar 화면으로 Fallback합니다. 따라서 Debug Visualization만 제거해도 기존 Geomorph 기능에 영향을 주지 않습니다.

`DiskTerrainManager`에는 `GeomorphDebugVisualization` bool과 Getter/Setter만 추가했습니다. `TerrainSettingsPanel`은 해당 bool을 `Geomorph Debug 색상` 체크박스에 연결합니다. Streaming, Worker, Cache, LOD Selector에는 Debug 관련 분기를 추가하지 않았습니다.

## 제거 방법

Debug Visualization만 제거하려면 다음 항목만 되돌리면 됩니다.

1. `TriplanarGeomorphDebugPS.hlsl` 제거
2. `TriplanarGeomorphVS.hlsl`의 `MorphProgress` 출력 제거
3. `QuadTreeLODGeomorphRenderer`의 Debug Shader 멤버와 `Begin(..., debugVisualization)` 분기 제거
4. `DiskTerrainManager`의 `GeomorphDebugVisualization` Getter/Setter 제거
5. `TerrainSettingsPanel.cpp`의 `Geomorph Debug 색상` 체크박스 제거
6. `.vcxproj/.filters`의 Debug PS 등록 제거

이 작업은 기존 `QuadTreeLOD`, `QuadTreeLODSelector`, `QuadTreeLODGeomorphBuilder`, `QuadTreeLODGeomorphWorker`, Tile/Streaming/Cache 코드를 되돌릴 필요가 없습니다.

## 확인 순서

1. Visual Studio 2022에서 `DXFramework.sln` → `Debug | x64` 빌드 및 실행.
2. `QuadTree LOD`와 `QuadTree Geomorphing`을 ON.
3. `Geomorph Debug 색상`을 ON.
4. 굴곡이 있는 지형을 낮은 각도에서 바라보고 카메라를 천천히 앞뒤로 이동.
5. LOD 전환 대상 Node가 **Red → Yellow → Green**으로 변하는지 확인.
6. Debug 색상을 OFF했을 때 기존 Triplanar Texture 화면으로 즉시 복귀하는지 확인.
7. `QuadTree Geomorphing` 자체를 OFF했을 때 기존 즉시 LOD 전환 경로가 유지되는지 확인.

이번 Debug 기능은 Geomorphing의 체감 차이를 인위적으로 크게 만드는 기능이 아니라, **현재 선택된 Node가 실제로 Morph 전이 중인지 확인하는 시각적 검증 도구**입니다.

## 검증 범위

프로젝트 구조, C++ 정적 컴파일 가능한 부분, 기존 Geomorph/Streaming 관련 플랫폼 독립 테스트, Visual Studio 프로젝트 XML, Shader 파일 등록, README 개수 및 ZIP 무결성을 확인합니다. 이 작업 환경에서는 Windows SDK/MSVC/Direct3D11 화면을 실제 실행할 수 없으므로 최종 색상 출력과 입력 동작은 사용자 PC의 `Debug | x64` 실행으로 확인해야 합니다.
