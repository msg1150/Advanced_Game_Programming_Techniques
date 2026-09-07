# QuadTree Frustum Culling Terrain

HeightMap + Height/Slope Texture Splatting Terrain을 **QuadTree로 공간 분할**하고, Camera Frustum에 보이지 않는 Terrain 영역을 렌더링에서 제외하는 단계입니다.

이번 단계의 핵심은 화면 결과를 바꾸는 것이 아니라 **보이지 않는 Triangle을 Draw하지 않는 것**입니다.

## 핵심 구현

- Terrain XZ 영역 QuadTree 분할
- 실제 Terrain 높이를 포함한 3D AABB
- Camera View / Projection 기반 World Frustum
- Parent Node 단위 재귀 Frustum Culling
- QuadTree Leaf 순서 기반 연속 Index Buffer
- Visible Index Range만 `DrawIndexed`
- Parent가 Frustum 내부에 완전히 포함되면 하위 Leaf를 하나의 Draw Range로 처리
- `F1` Culling ON / OFF
- 화면 Debug Overlay
- Visible Leaves / Culled Nodes / Rendered Triangles / Draw Calls 표시
- 기존 Height/Slope Texture Splatting 유지

## 실행 화면 Debug UI

화면 좌측 상단에 항상 다음 정보가 표시됩니다.

```text
[F1] QuadTree Culling : ON
Visible Leaves : 72 / 256    Culled Nodes : 13
Rendered Triangles : 9216 / 32768    Draw Calls : 18
```

`F1`을 누르면:

```text
ON
↕
OFF
```

가 즉시 전환되며 화면의 글씨도 같이 바뀝니다.

## 중요한 비교 기준

Culling ON/OFF의 **Terrain 모양은 동일해야 정상**입니다.

차이는 렌더링되는 Triangle 개수입니다.

```text
Culling OFF
→ Terrain 전체 Triangle 렌더링

Culling ON
→ Camera Frustum과 겹치는 QuadTree 영역만 렌더링
```

OFF 상태에서는 전체 Terrain을 하나의 Root Index Range로 Draw하기 때문에 Draw Call은 1회입니다.

따라서 이번 단계에서 Culling 성능 효과를 판단할 때는 **Draw Call 감소가 아니라 Rendered Triangles 감소를 우선적으로 확인**합니다.

## 구조

```text
Source/
├─ Graphics/
│  ├─ Frustum.h / .cpp
│  ├─ DebugTextRenderer.h / .cpp
│  └─ 기존 Graphics...
│
└─ Features/
   └─ QuadTreeCulling/
      ├─ QuadTree.h
      ├─ QuadTree.cpp
      ├─ QuadTreeCulling.h
      ├─ QuadTreeCulling.cpp
      ├─ QuadTreeTerrain.h
      └─ QuadTreeTerrain.cpp
```

## 전체 처리 흐름

```text
HeightMap
   ↓
HeightMapTerrainGenerator
   ↓
Vertex / Normal / UV
   ↓
QuadTree Build
   ↓
Leaf 순서로 Index 재배치
   ↓
Camera View / Projection
   ↓
World Frustum
   ↓
QuadTree AABB 검사
   ↓
Visible Index Ranges
   ↓
Texture Splat Material
   ↓
DrawIndexed Range
```

## QuadTree 분할

Terrain 전체 Cell 영역을 재귀적으로 분할합니다.

```text
┌─────────────┬─────────────┐
│             │             │
│     NW      │     NE      │
│             │             │
├─────────────┼─────────────┤
│             │             │
│     SW      │     SE      │
│             │             │
└─────────────┴─────────────┘
```

현재 테스트 HeightMap:

```text
129 × 129 Vertex
→ 128 × 128 Cell
```

기본 Leaf 크기:

```cpp
quadTreeSettings.QuadTree.LeafCellSize = 8;
```

이므로 최대:

```text
16 × 16
= 256 Leaf
```

가 생성됩니다.

## Node AABB

QuadTree Node의 Bounds는 XZ 평면만 사용하지 않습니다.

각 Node에 포함된 실제 Terrain Vertex를 조사하여:

```text
Min X / Max X
Min Y / Max Y
Min Z / Max Z
```

를 계산합니다.

따라서 높은 산이나 골짜기도 Bounds 안에 포함됩니다.

## Frustum

`Graphics/Frustum`은 Camera의:

```text
View Matrix
Projection Matrix
```

를 이용해 World Space Frustum을 만듭니다.

각 Node의 AABB와 비교해 다음 상태를 얻습니다.

```text
DISJOINT
→ Node 전체 화면 밖
→ Child 검사 없이 제거

INTERSECTS
→ 일부 화면 안
→ Child 계속 검사

CONTAINS
→ Node 전체 화면 안
→ 하위 Leaf를 하나의 연속 Index Range로 Draw
```

## 연속 Index Range

QuadTree를 Build할 때 Leaf 순서대로 Index를 저장합니다.

```text
Index Buffer

[Leaf 0]
[Leaf 1]
[Leaf 2]
[Leaf 3]
...
```

Parent Node는 자신의 하위 Leaf 전체가 차지하는:

```text
StartIndex
IndexCount
```

를 가지고 있습니다.

따라서 Parent가 Frustum 안에 완전히 들어왔을 때 Child를 하나씩 Draw하지 않고 Parent Range 하나로 묶을 수 있습니다.

## Mesh::DrawRange

기존 `Mesh::Draw()`는 그대로 유지하고 공용 기능을 하나 추가했습니다.

```cpp
Mesh::DrawRange(
    context,
    indexCount,
    startIndex);
```

내부적으로는:

```cpp
DrawIndexed(
    indexCount,
    startIndex,
    0);
```

를 사용합니다.

이 기능은 QuadTree뿐 아니라 이후 SubMesh / LOD에도 재사용할 수 있습니다.

## F1 Toggle

`Application::Update()`에서:

```text
F1 Pressed
   ↓
현재 Culling 상태 반전
   ↓
QuadTreeTerrain::SetCullingEnabled()
```

순서로 처리합니다.

키를 누르고 있는 동안 계속 토글되지 않고, 한 번 눌렀을 때 한 번만 상태가 변경됩니다.

## Debug Text Overlay

QuadTree 상태를 Window Title이 아니라 **실제 렌더링 화면 위에 표시**합니다.

공용 클래스:

```text
Graphics/DebugTextRenderer
```

사용 기술:

```text
Direct2D
DirectWrite
```

이 기능을 위해 Renderer의 D3D11 Device 생성에:

```text
D3D11_CREATE_DEVICE_BGRA_SUPPORT
```

를 추가했습니다.

추가 Link Library:

```text
d2d1.lib
dwrite.lib
```

Window Resize 시 Direct2D가 참조하는 기존 BackBuffer를 먼저 해제한 뒤 SwapChain Resize가 끝나면 다시 생성합니다.

## Culling 통계

### Visible Leaves

현재 Frustum 안에 포함되어 렌더링되는 Leaf 수입니다.

```text
Visible Leaves : 63 / 256
```

### Culled Nodes

Frustum 밖으로 판정되어 하위 검사 자체가 생략된 Node 수입니다.

### Rendered Triangles

이번 Frame에 실제 Draw Range에 포함된 Triangle 수입니다.

```text
Rendered Triangles : 8192 / 32768
```

Culling의 효과를 확인할 때 가장 중요한 값입니다.

### Draw Calls

이번 Frame의 `DrawIndexed` 호출 횟수입니다.

QuadTree Culling은 공간 영역별 부분 Draw를 수행하므로 Culling OFF의 1 Draw Call보다 ON의 Draw Call이 많을 수 있습니다.

이번 단계의 목적은 Draw Call 자체를 줄이는 것이 아니라 **Camera에 보이지 않는 Terrain Triangle을 제외하는 것**입니다.

## 기존 Texture Splatting 유지

QuadTreeTerrain은 이전 단계의:

```text
Grass
Rock
Snow

Height 기반 Weight
Slope 기반 Weight
```

를 그대로 사용합니다.

즉 Culling ON/OFF에 따라 Texture 결과가 달라지면 정상적인 상태가 아닙니다.

## 기존 코드 영향 최소화

### 공용 Framework 추가

```text
Graphics/Frustum
Graphics/DebugTextRenderer
Mesh::DrawRange
Renderer::GetSwapChain
D3D11_CREATE_DEVICE_BGRA_SUPPORT
```

### 새 Feature

```text
Features/QuadTreeCulling
```

### 기존 구현 유지

```text
HeightMapImage
HeightMapTerrainGenerator
HeightMapTerrain

PerlinTerrain

TextureSplatMaterial
Texture2D
```

Texture Splatting의 Height / Slope 계산 방식도 변경하지 않습니다.

## 현재 단계에서 제외

```text
LOD
Distance Culling
Occlusion Culling
GPU Culling
Chunk Streaming
Terrain Paging
LOD Stitching
```

QuadTree는 이번 단계에서 **Frustum Culling만 담당**합니다.

## 정상 동작 확인

1. 실행 직후 Overlay에 `QuadTree Culling : ON` 표시
2. Camera 회전 / 이동
3. `Visible Leaves`와 `Rendered Triangles` 값 변화
4. `F1` 입력
5. `QuadTree Culling : OFF` 표시
6. `Rendered Triangles = Total Triangles`
7. 다시 `F1`
8. ON 상태에서 화면 밖 Terrain이 생기면 Rendered Triangles 감소
9. ON/OFF 전환 시 Terrain의 보이는 형태 자체는 동일

## 조작

| 입력 | 기능 |
|---|---|
| `W / A / S / D` | Camera 이동 |
| `좌클릭 + Drag` | Orbit 회전 |
| `마우스 휠` | Zoom |
| `F1` | QuadTree Culling ON / OFF |

## 인코딩

```text
.cpp / .h
→ UTF-8 BOM

.hlsl
→ UTF-8 without BOM
```
