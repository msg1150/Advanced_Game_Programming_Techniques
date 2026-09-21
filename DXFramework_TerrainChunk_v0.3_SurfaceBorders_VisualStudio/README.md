# Terrain Chunk System v0.3 — Surface Borders

이 단계는 **Triplanar Terrain v0.1** 프로젝트를 기반으로, 하나의 HeightMap을 여러 Chunk로 분할하고 각 Chunk에 독립 Mesh / Bounds / QuadTree LOD / Frustum Culling을 적용합니다. **Streaming 기능은 아직 없습니다.** 모든 Chunk 리소스는 시작 시 생성합니다.

## 구조

```text
원본 HeightMap 129 x 129 Vertices (128 x 128 Cells)
  └─ HeightMapTerrainGenerator: 전체 Vertex/UV/Normal을 한 번 생성
      └─ TerrainChunkGenerator: 32 x 32 Cells 단위로 4 x 4 = 16 Chunk 분할
          └─ TerrainChunkManager
              ├─ Chunk별 Mesh + QuadTreeLOD + Bounds
              ├─ Frustum으로 Chunk 단위 1차 제거
              ├─ 기존 QuadTreeLODSelector로 2차 Node Culling/LOD
              └─ 모든 Chunk에서 단일 TriplanarMaterial 공유
```

## 새로운 파일

```text
Source/Features/TerrainChunk/
├─ TerrainChunkGenerator.h/.cpp  # CPU 분할 전담
└─ TerrainChunkManager.h/.cpp    # Chunk 리소스, Culling/LOD, Render
```

기존 HeightMap/Triplanar/QuadTreeLOD Feature 구현은 수정하지 않았습니다. 공유 Graphics 변경은 DebugTextRenderer의 **compact 표시 옵션** 1개뿐입니다. `Application`에서 신규 Chunk Feature와 단축키만 연결합니다.

### Chunk의 경계가 이어지는 방식

전체 HeightMap Generator가 **한 번에 Vertex Normal을 계산**하고, Chunk Generator는 해당 값을 그대로 복사합니다. 인접 Chunk는 경계 Vertex 한 줄을 함께 포함(+1)하므로 동일한 Position/UV/Normal을 가집니다. Chunk별 Normal을 독립적으로 재계산하지 않아 경계에 색/법선 이음새가 생기는 문제를 줄였습니다. QuadTree LOD 경계에는 기존 Skirt가 적용됩니다. 서로 다른 LOD의 순간 전환에 따른 팝핑/높이 단순화는 남을 수 있습니다.

현재 Chunk Vertex Position은 *전체 Terrain 기준 Local 좌표*를 유지하며, Chunk 별도 World Transform 대신 Manager의 World Transform을 공유합니다. Chunk는 서로 다른 공간의 Vertex/Index Buffer 및 Bounding Box를 독립적으로 갖습니다.

## 단축키

| 키 | 기능 |
|---|---|
| `1` | Cube 표시 토글 |
| `2` | Perlin 표시 토글 |
| `3` | HeightMap 표시 토글 |
| `4` | QuadTree LOD Terrain 표시 토글 |
| `5` | Triplanar Terrain 표시 토글 |
| `6` | Terrain Chunks 표시 토글 |
| `7~0` | 예약 슬롯 |
| `F1` | Culling 토글 (QuadTree LOD / Triplanar / Chunk 동기화) |
| `F2` | LOD 토글 (위와 동일) |
| **`F3`** | **큰 디버그 정보 전체 표시/숨김** |
| **`F4`** | **Chunk 표면 경계선 표시/숨김 (기본 ON)** |

**실행 기본값: 1~5 OFF, 6 ON, F3 Debug Overlay OFF.** 좌측 상단의 짧은 `[F3] Debug Info` 안내만 남습니다. F3을 누르면 숫자키별 상태와 Chunk 통계가 펼쳐지고 다시 누르면 접힙니다. 기존 Direct2D 렌더러에서 compact 박스만 선택적으로 그리므로 숨김 상태에 큰 배경 박스가 남지 않습니다.

## Debug 통계

- `Chunks Visible / Total`: 실제 Draw Range가 1개 이상 있는 Chunk / 전체 Chunk.
- `Culled`: Frustum 검사로 제외되거나 내부 Node가 전부 제외된 Chunk.
- `Active Nodes`: 선택된 QuadTree 렌더링 Node 수.
- `Draw Calls`: Chunk의 실제 DrawIndexed 호출 수. Chunk를 나누면 Draw Call은 늘 수 있습니다.
- `Surface Triangles`: Skirt를 제외한 실제 선택된 지형 Triangle / 전체 원본 Triangle. 129×129 기준 전체는 **32768개**.

## 테스트 절차

1. 실행 후 숫자키 `6`으로 Chunk Terrain이 보였다 사라지는지 확인.
2. `F3`으로 큰 Debug 박스 표시/숨김 테스트. 숨기면 작은 안내만 남아야 함.
3. `6 ON, F1 OFF, F2 OFF` -> Chunks `16 / 16`, Surface Triangles `32768 / 32768`, Draw Calls `16`이 기준. (Terrain 전체를 초기화했다는 뜻이지 16개 모두 화면에 보인다는 뜻은 아님.)
4. `F1 ON, F2 OFF` -> 카메라 밖 Chunk 및 내부 Node가 제외됨.
5. `F1 OFF, F2 ON` -> 카메라와 멀어지면 Surface Triangles가 감소하고 가까우면 증가함.
6. Chunk 경계에서 구멍/수직 이음새가 없는지 확인. Skirt로 LOD 경계의 틈을 가리지만 모든 시점/지형에서 완벽함을 보장하지는 않음.
7. `5`를 켜면 기존 Triplanar Terrain과 비교 가능. 이전 샘플은 원점에서 옆으로 이동됨.

## 설정 위치

`Source/Core/Application.cpp`의 `TerrainChunkSettings`:

```cpp
chunkSettings.CellsPerChunk = 32u; // 128 x 128 Cells -> 4 x 4 Chunks
chunkSettings.LOD = lodTerrainSettings.LOD;
chunkSettings.EnableCulling = lodTerrainSettings.EnableCulling;
chunkSettings.EnableLOD = lodTerrainSettings.EnableLOD;
```

마지막 Chunk는 남는 Cell 크기만큼 자동 생성하므로, 32로 나누어떨어지지 않는 HeightMap도 지원합니다.

## 제거 방법

`Source/Features/TerrainChunk/` 4개 파일 및 `.vcxproj/.filters/CMakeLists.txt` 등록을 삭제하고 `Application.h/.cpp`의 `terrainChunkManager_`, 6번 슬롯, Render, F1/F2 동기화 연결부를 제거하면 이전 Triplanar Terrain으로 돌아갑니다. F3 기능까지 없애려면 `showDebugOverlay_`와 DebugTextRenderer의 `compact` 파라미터를 원복합니다. 기존 Terrain/QuadTree/Triplanar 코드는 그대로 남습니다.

## 이 단계에서 제외

Streaming, 비동기 로딩, LOD Stitching/Geomorphing, GPU Culling, Chunk 전용 파일 포맷, 실제 대형 HeightMap 검증. Chunk마다 GPU Buffer를 만들므로 단일 Mesh보다 Draw Call과 메모리 오버헤드가 커질 수 있습니다.

## 인코딩/빌드

`.cpp/.h`: UTF-8 BOM, `/utf-8` / Unicode. `.hlsl`: UTF-8 **without** BOM. Windows Visual Studio 2022에서 `DXFramework.sln`을 열어 `Debug | x64`로 빌드하세요. 이 프로젝트의 실제 MSVC 빌드와 화면 실행 검증은 사용자 환경에서 확인해야 합니다.

## v0.3: Terrain 표면 경계선 수정

v0.2에서는 각 Chunk의 Bounding Box 최상단 `MaxY`로 수평 사각형을 그려,
산과 평지의 높이 차이 때문에 경계선이 공중에 떠 있었습니다. **실제 Chunk Mesh의 높이 불일치라고 단정할 수 없는 Debug 표현 문제**였습니다.

v0.3에서는 `TerrainChunkGenerator`가 분리한 CPU Chunk의 **원본 HeightMap Vertex Position**에서
북쪽/서쪽 경계의 모든 Grid Vertex를 차례대로 가져와 작은 Line List 구간으로 연결합니다.
전체 Terrain 외곽의 오른쪽/아래쪽 경계는 마지막 열/행에서만 추가합니다.
인접 Chunk 사이 공유 Edge가 중복 렌더링되어 다른 색이 겹치는 현상을 방지합니다.

```text
이전: AABB의 가장 높은 Y -> 공중에 뜬 수평 사각형
현재: 각 Edge Vertex의 실제 Y -> 경사와 산의 높이를 따라가는 경계선
```

- 경계선만 원본 높이보다 `0.025` World Unit 위로 올려 Z-Fighting을 줄입니다.
- 작은 Depth Bias가 적용된 별도 Line Rasterizer를 사용하며, `F4` OFF면 경계선 Pass를 생략합니다.
- 색상은 기존처럼 Chunk 좌표에 따라 주황/청록을 번갈아 사용합니다. **Chunk 번호는 표시하지 않습니다.**
- `F3`의 작은 안내는 박스 너비에 맞춰 `[F3] Info | F4:ON/OFF`로 줄여 잘림을 막습니다.
- `F1` Culling의 영향을 받는 Chunk만 경계선이 그려집니다. Debug 경계선 Draw Call은 Terrain 통계에서 제외됩니다.
- 주의: **LOD ON 시** 실제 표면은 단순화된 LOD Mesh인데 경계선은 원본 HeightMap을 따릅니다.
  따라서 강한 LOD 단순화가 적용된 곳은 선 일부가 표면 뒤에 가려질 수 있습니다.
  경계의 정확한 위치/높이 확인은 `6 ON, F1 OFF, F2 OFF, F4 ON` 상태에서 수행하세요.

### 이번 수정 파일과 제거 방법

- `Source/Features/TerrainChunk/TerrainChunkManager.h/.cpp`: Debug 경계 Mesh 생성 입력을 `BoundingBox`에서 CPU Chunk Vertex로 변경하고 공유 Edge 중복을 제거.
- `Source/Core/Application.cpp`: 짧은 F3 안내 문구 수정.
- 기존 HeightMap 생성, Chunk 분할, QuadTree/LOD, Triplanar, Renderer, Shader, 빌드 설정은 변경하지 않았습니다.
- 경계 시각화만 제거할 경우 `TerrainChunkManager`의 `BoundsMesh`, Shader, F4 Getter/Setter 및 Debug Pass와 `Application`의 F4 연결만 제거하면 됩니다.

**실제 Windows/MSVC DirectX 빌드와 게임 화면 검증은 사용자 환경에서 진행해야 합니다.**
