# QuadTree Distance LOD Terrain

이 단계는 이전의 **HeightMap + Height/Slope Texture Splatting + QuadTree Frustum Culling**을 유지하면서, Camera 거리에 따라 Terrain의 Geometry 해상도를 자동으로 조절하는 **QuadTree LOD**를 추가한 단계입니다.

이번 구현에서 가장 중요한 설계 원칙은 **기존 기능을 최대한 수정하지 않고 LOD를 독립 Feature로 추가하는 것**입니다.

## 핵심 결과

```text
F1
→ Frustum Culling ON / OFF

F2
→ QuadTree LOD ON / OFF
```

화면 좌측 상단에서 두 기능의 상태와 통계를 바로 확인할 수 있습니다.

```text
[F1] Culling : ON    [F2] QuadTree LOD : ON
Active Nodes : ...
Culled Nodes : ...
Surface Triangles : ... / 32768
LOD Nodes 0 / 1 / 2 / 3+ : ...
```

## 기존 코드 유지 원칙

이전 단계의 다음 구현은 그대로 유지합니다.

```text
Features/QuadTreeCulling
Features/HeightMapTerrain
Features/TextureSplatting

Graphics/Frustum
Graphics/Mesh
Graphics/Texture2D
```

특히 기존:

```text
Source/Features/QuadTreeCulling/
```

은 LOD 구현을 위해 수정하지 않았습니다.

새로운 LOD 코드는 별도 폴더에만 추가합니다.

```text
Source/Features/QuadTreeLOD/
├─ QuadTreeLOD.h
├─ QuadTreeLOD.cpp
├─ QuadTreeLODSelector.h
├─ QuadTreeLODSelector.cpp
├─ QuadTreeLODTerrain.h
└─ QuadTreeLODTerrain.cpp
```

따라서 LOD 기능을 제거할 경우:

```text
1. Source/Features/QuadTreeLOD 제거
2. Application의 QuadTreeLODTerrain 연결부 제거/이전 QuadTreeTerrain으로 복구
3. vcxproj / filters에서 QuadTreeLOD 항목 제거
```

만 하면 이전 QuadTree Culling 단계 구조로 돌아갈 수 있습니다.

## 전체 구조

```text
HeightMap
   ↓
HeightMapTerrainGenerator
   ↓
Position / Normal / UV
   ↓
QuadTreeLOD Build
   │
   ├─ Full Resolution Range
   └─ Node별 LOD Patch Range
          ↓
Camera
   │
   ├─ Frustum
   └─ World Position
          ↓
QuadTreeLODSelector
   │
   ├─ Frustum Culling
   └─ Distance LOD
          ↓
Visible / Selected Node
          ↓
Texture Splat Material
          ↓
DrawIndexed Range
```

## LOD 방식

각 QuadTree Node는 자기 영역을 일정한 Patch 해상도로 표현합니다.

현재 설정:

```cpp
lodTerrainSettings.LOD.LeafCellSize = 8u;
```

129x129 HeightMap은:

```text
128 x 128 Cell
```

이므로 QuadTree는 다음처럼 세분화됩니다.

```text
LOD4
128 Cell 영역
Root
   ↓

LOD3
64 Cell 영역
   ↓

LOD2
32 Cell 영역
   ↓

LOD1
16 Cell 영역
   ↓

LOD0
8 Cell 영역
Leaf / 가장 세밀함
```

중요한 점은 **LOD0이 가장 세밀하고 숫자가 커질수록 더 낮은 해상도**라는 것입니다.

## 같은 Patch 해상도로 서로 다른 공간을 표현

각 Node는 최대 약 `8 x 8 Cell Patch`로 자기 영역을 표현합니다.

예:

```text
Root
128 x 128 Cell 공간
→ 약 8 x 8 Patch로 표현

Child
64 x 64 Cell 공간
→ 약 8 x 8 Patch로 표현

Leaf
8 x 8 Cell 공간
→ 8 x 8 Patch
→ 원본 해상도
```

따라서 Parent를 사용하면 적은 Triangle으로 넓은 Terrain을 표현하고, Child로 내려갈수록 같은 면적당 Triangle 수가 증가합니다.

## 거리 기반 분할

Node의 Center까지 단순 거리만 사용하지 않고 **Camera와 Node AABB 사이의 최단 거리**를 사용합니다.

```text
SplitDistance
=
NodeWorldSize
×
SplitDistanceFactor
```

현재 기본값:

```cpp
lodTerrainSettings.LOD.SplitDistanceFactor =
    3.0f;
```

판정:

```text
Camera가 SplitDistance 안쪽
→ Child로 내려감
→ 더 높은 Detail

Camera가 SplitDistance 바깥
→ 현재 Parent에서 멈춤
→ 더 낮은 Detail
```

`SplitDistanceFactor`를 크게 하면 더 먼 거리에서도 높은 Detail을 유지합니다.

## F2 LOD OFF

`F2`로 LOD를 끄면 Distance LOD 선택을 하지 않습니다.

```text
LOD OFF
→ QuadTree Leaf까지 무조건 내려감
→ LOD0 / 원본 Grid 해상도
```

따라서 F2를 누르면서 `Surface Triangles`를 비교하면 LOD 효과를 바로 확인할 수 있습니다.

## F1 Culling과 독립

Culling과 LOD는 서로 독립적으로 동작합니다.

### F1 ON / F2 ON

```text
Frustum 밖
→ 제거

Frustum 안
→ 거리 기준 LOD
```

### F1 OFF / F2 ON

```text
Terrain 전체
→ 거리 기준 LOD
```

### F1 ON / F2 OFF

```text
Frustum 밖
→ 제거

Frustum 안
→ Full Resolution Leaf
```

### F1 OFF / F2 OFF

```text
Terrain 전체
→ Full Resolution
→ 전체 Index Range 1회 Draw
```

이 조합으로 Culling과 LOD의 효과를 각각 따로 비교할 수 있습니다.

## Crack 방어

서로 인접한 Node가 다른 LOD를 사용하면 경계 Vertex 밀도가 달라집니다.

```text
LOD0            LOD2
|_|_|_|_|       |___|___|
```

Height 차이가 있는 Terrain에서는 이 경계에 작은 틈이 생길 수 있습니다.

이번 단계에서는 기존 Terrain Vertex나 Generator를 수정하지 않고 **Feature Local Skirt** 방식으로 처리합니다.

```text
Terrain Edge
────────────
│
│ Skirt
│
↓
```

각 LOD Patch의 외곽 Vertex를 조금 아래로 복제하여 틈이 보이지 않게 가립니다.

현재 설정:

```cpp
lodTerrainSettings.LOD.SkirtDepth =
    0.18f;
```

Skirt는 `Source/Features/QuadTreeLOD` 내부에서만 생성됩니다.

## Surface Triangle 통계

화면의:

```text
Surface Triangles : 현재 / 전체
```

값은 **Skirt Triangle을 제외한 실제 Terrain 표면 Triangle만 계산**합니다.

따라서 기존 Full Resolution Terrain과 LOD 효과를 직접 비교하기 쉽습니다.

129x129 HeightMap 기준 Full Resolution:

```text
128 x 128 Cell
x
2 Triangle

=
32768 Surface Triangles
```

LOD가 켜지면 Camera 거리에 따라 이 값이 감소합니다.

## Actual Draw와 Skirt

GPU에 실제로 들어가는 Index Range에는 Crack 방어용 Skirt도 포함됩니다.

따라서 내부적으로 실제 Draw되는 Triangle 수는 `Surface Triangles`보다 조금 많을 수 있습니다.

하지만 화면 Debug UI에서는 LOD 자체의 감소 효과를 명확히 보기 위해 Surface Triangle 수를 기준으로 표시합니다.

## 기존 Texture Splatting 유지

LOD가 바뀌어도 이전 단계의:

```text
낮고 완만함
→ Grass

가파름
→ Rock

높고 완만함
→ Snow
```

규칙은 그대로 사용합니다.

새로운 LOD Terrain도 기존:

```text
TextureSplatMaterial
TextureSplatVS
TextureSplatPS
```

를 그대로 재사용합니다.

## Debug Overlay

### 첫 번째 줄

```text
[F1] Culling : ON/OFF
[F2] QuadTree LOD : ON/OFF
```

### 두 번째 줄

```text
Active Nodes
Culled Nodes
Draw Calls
```

### 세 번째 줄

```text
Surface Triangles : 현재 / Full Resolution
```

### 네 번째 줄

```text
LOD Nodes 0 / 1 / 2 / 3+
```

카메라에 가까이 가면 일반적으로:

```text
LOD0 / LOD1 증가
Surface Triangles 증가
```

멀어지면:

```text
LOD2 / LOD3+ 증가
Surface Triangles 감소
```

하는 것이 정상입니다.

## 설정 위치

`Source/Core/Application.cpp`

```cpp
lodTerrainSettings.LOD.LeafCellSize =
    8u;

lodTerrainSettings.LOD.SplitDistanceFactor =
    3.0f;

lodTerrainSettings.LOD.SkirtDepth =
    0.18f;

lodTerrainSettings.EnableCulling =
    true;

lodTerrainSettings.EnableLOD =
    true;
```

## 조작

| 입력 | 기능 |
|---|---|
| `W / A / S / D` | Camera Target 이동 |
| `좌클릭 + Drag` | Orbit |
| `마우스 휠` | Zoom |
| `F1` | Frustum Culling ON / OFF |
| `F2` | QuadTree LOD ON / OFF |

## 정상 동작 기준

다음 조건을 확인합니다.

```text
1. F1을 누르면 Culling 상태가 바뀐다.
2. F2를 누르면 LOD 상태가 바뀐다.
3. 두 상태가 화면 Overlay에서 바로 보인다.
4. LOD ON 상태에서 Camera 거리에 따라 LOD Node 분포가 바뀐다.
5. 멀어질수록 Surface Triangles가 감소한다.
6. 가까워질수록 LOD0/LOD1 사용량과 Surface Triangles가 증가한다.
7. LOD OFF에서는 보이는 영역이 Full Resolution Leaf로 렌더링된다.
8. F1 OFF + F2 OFF에서는 전체 Terrain이 Full Resolution으로 렌더링된다.
9. Texture Splatting 결과가 유지된다.
10. 서로 다른 LOD 경계에서 큰 Crack이 보이지 않는다.
```

## 이번 단계에서 제외

```text
Geomorphing
Neighbor Stitch Index
Continuous LOD
CDLOD 완전체
GPU Driven LOD
Compute Shader Culling
Mesh Shader
Nanite-like Cluster Rendering
Chunk Streaming
```

이번 단계의 목적은 **전통적인 QuadTree 기반 Distance LOD의 구조와 효과를 명확하게 구현하고 검증하는 것**입니다.

## DPI 관련 수정

이전 QuadTree Culling 테스트에서 최신 Windows SDK의:

```text
ID2D1Factory::GetDesktopDpi()
C4996 Deprecated
```

문제가 있었기 때문에 이번 프로젝트에는 해당 수정도 반영되어 있습니다.

현재 Debug Overlay는:

```cpp
GetDpiForWindow()
```

를 사용합니다.

## 인코딩

```text
.cpp / .h
→ UTF-8 BOM

.hlsl
→ UTF-8 without BOM
```
