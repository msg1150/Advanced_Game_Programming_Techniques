# Triplanar Mapping Terrain

이번 단계는 이전에 구현한 **HeightMap + Height/Slope Texture Splatting + QuadTree Frustum Culling + QuadTree LOD** 구조를 유지하면서, Terrain의 가파른 면에서 발생할 수 있는 Texture Stretching을 줄이기 위해 **Triplanar Mapping**을 추가한 단계입니다.

## 핵심 목표

기존 Texture Splatting에서는 Terrain UV를 사용해 Grass / Rock / Snow Texture를 Sample했습니다.

```text
Terrain UV
→ Texture Sample
```

이 방식은 평지에서는 잘 보이지만, 절벽처럼 Surface가 수직에 가까워질수록 Texture가 한 방향으로 늘어나 보일 수 있습니다.

Triplanar Mapping은 같은 Texture를:

```text
X 방향
Y 방향
Z 방향
```

세 방향에서 투영하고 Surface Normal을 기준으로 Blend합니다.

```text
World Position
+
World Normal
    ↓
X / Y / Z Projection
    ↓
Normal 기반 Projection Weight
    ↓
Triplanar Texture
```

## 이번 단계의 확장성 원칙

기존 코드는 최대한 유지합니다.

수정하지 않은 핵심 기능:

```text
HeightMapTerrain
TextureSplatting
QuadTreeCulling
QuadTreeLOD
Texture2D
Mesh
Renderer
```

새 기능은 별도 폴더로 추가했습니다.

```text
Source/Features/TriplanarTerrain/
├─ TriplanarMaterial.h
├─ TriplanarMaterial.cpp
├─ TriplanarTerrain.h
└─ TriplanarTerrain.cpp

Shaders/TriplanarTerrain/
├─ TriplanarVS.hlsl
└─ TriplanarPS.hlsl
```

따라서 Triplanar 기능을 제거하려면:

```text
1. Source/Features/TriplanarTerrain 제거
2. Shaders/TriplanarTerrain 제거
3. Application의 Include / 멤버 / Initialize / Render 연결부 제거
4. 프로젝트 파일 등록 항목 제거
```

하면 됩니다.

기존 QuadTree LOD Terrain은 그대로 유지됩니다.

## 기존 시스템 재사용

새 Triplanar Terrain은 다음 기존 기능을 그대로 사용합니다.

```text
HeightMapImage
HeightMapTerrainGenerator
QuadTreeLOD
QuadTreeLODSelector
Frustum
Mesh
Texture2D
```

즉 Terrain Geometry / LOD / Culling 알고리즘은 새로 만들지 않았습니다.

변경된 핵심은 Material / Shader입니다.

```text
기존
TextureSplatMaterial
+
TextureSplat Shader

새 기능
TriplanarMaterial
+
Triplanar Shader
```

## Triplanar Projection

### X Projection

X 방향을 향하는 Surface는 YZ 평면으로 Texture를 투영합니다.

```hlsl
float2 uvX =
    worldPosition.zy *
    ProjectionScale;
```

### Y Projection

위쪽/아래쪽 Surface는 XZ 평면을 사용합니다.

```hlsl
float2 uvY =
    worldPosition.xz *
    ProjectionScale;
```

Terrain 평지는 대부분 Y Projection 비중이 높습니다.

### Z Projection

Z 방향 Surface는 XY 평면을 사용합니다.

```hlsl
float2 uvZ =
    worldPosition.xy *
    ProjectionScale;
```

## Projection Weight

Surface Normal의 절대값을 이용합니다.

```hlsl
float3 weights =
    pow(
        abs(normal),
        BlendSharpness);
```

예를 들어:

```text
Normal ≈ (0, 1, 0)
→ Y Projection 비중 큼

Normal ≈ (1, 0, 0)
→ X Projection 비중 큼

Normal ≈ (0, 0, 1)
→ Z Projection 비중 큼
```

세 Weight는 합이 1이 되도록 정규화합니다.

## BlendSharpness

현재 기본값:

```cpp
triplanarMaterialDesc.BlendSharpness =
    4.0f;
```

값이 작으면:

```text
X/Y/Z Projection이 넓게 섞임
```

값이 커지면:

```text
Surface가 가장 가까이 바라보는 Projection이 더 강해짐
```

## ProjectionScale

현재 기본값:

```cpp
triplanarMaterialDesc.ProjectionScale =
    2.0f;
```

Triplanar Mapping은 기존 Terrain UV 대신 World Position을 사용하기 때문에 `TextureTiling` 대신 `ProjectionScale`을 사용합니다.

```text
값 증가
→ Texture가 더 자주 반복
→ Texture 무늬가 작아짐

값 감소
→ Texture가 덜 반복
→ Texture 무늬가 커짐
```

## Height / Slope Texture Splatting 유지

이번 단계는 Texture Layer 선택 규칙을 바꾸는 단계가 아닙니다.

이전 규칙을 그대로 사용합니다.

```text
낮고 완만한 지형
→ Grass

가파른 지형
→ Rock

높고 완만한 지형
→ Snow
```

차이는 Texture를 **어떻게 표면에 투영하느냐**입니다.

```text
기존
Layer 선택
→ Terrain UV Sample

Triplanar
Layer 선택
→ X/Y/Z World Projection
→ Projection Blend
```

## QuadTree LOD 유지

Triplanar Terrain도 기존 QuadTree LOD를 그대로 사용합니다.

```text
Camera 가까움
→ 높은 Geometry Detail

Camera 멂
→ 낮은 Geometry Detail
```

또한:

```text
F1
→ Frustum Culling

F2
→ QuadTree LOD
```

도 그대로 동작합니다.

## Showcase 숫자키

화면이 너무 복잡해지는 것을 막기 위해 이전 단계에서 만든 숫자키 토글을 그대로 확장했습니다.

```text
[1] Cube
[2] Perlin Terrain
[3] HeightMap Terrain
[4] QuadTree LOD Terrain
[5] Triplanar Terrain
[6] Reserved
[7] Reserved
[8] Reserved
[9] Reserved
[0] Reserved
```

### 기본 표시 상태

이번 단계에서는 새 기능을 바로 확인할 수 있도록:

```text
1 Cube                 OFF
2 Perlin Terrain       OFF
3 HeightMap Terrain    OFF
4 QuadTree LOD Terrain OFF
5 Triplanar Terrain    ON
```

으로 시작합니다.

원하는 항목만 숫자키로 켜서 비교하면 됩니다.

## 비교 방법

Triplanar 효과를 가장 쉽게 확인하려면:

```text
4
→ 기존 QuadTree LOD Terrain ON

5
→ Triplanar Terrain ON
```

으로 두 Terrain을 번갈아 확인합니다.

특히 **가파른 Rock 절벽 부분**을 보는 것이 중요합니다.

### 기존 UV Mapping

```text
절벽
→ Texture가 길게 늘어져 보일 가능성
```

### Triplanar Mapping

```text
절벽 Normal 방향에 맞는 X/Z Projection 증가
→ Texture Stretching 감소
```

## Debug Overlay

F1 / F2 상태는 기존과 동일하게 표시합니다.

LOD 통계는:

```text
Triplanar Terrain이 ON
→ Triplanar Terrain 통계 우선 표시

Triplanar OFF + QuadTree LOD Terrain ON
→ 기존 QuadTree LOD Terrain 통계 표시
```

방식입니다.

Overlay에:

```text
Stats Source : Triplanar Terrain
```

또는:

```text
Stats Source : QuadTree LOD Terrain
```

이 표시되어 어떤 Terrain 통계인지 구분할 수 있습니다.

## 전체 파이프라인

```text
HeightMap
   ↓
Terrain Geometry
   ↓
QuadTree LOD
   ↓
Frustum Culling
   ↓
Visible LOD Range
   ↓
World Position / World Normal
   ↓
Height / Slope Layer Weight
   ↓
Grass / Rock / Snow
   ↓
각 Layer Triplanar X/Y/Z Projection
   ↓
Normal 기반 Projection Blend
   ↓
Final Terrain
```

## 현재 단계에서 제외

```text
Normal Mapping
PBR
Directional Lighting
Virtual Texturing
Runtime Deformation
GPU Driven Terrain
Mesh Shader
Nanite-like Cluster Rendering
```

이번 단계 목적은 **기존 Terrain 최적화 구조를 유지하면서 절벽 Texture Projection 품질을 개선하는 것**입니다.

## 조작

| 입력 | 기능 |
|---|---|
| `W / A / S / D` | Camera Target 이동 |
| `좌클릭 + Drag` | Orbit |
| `마우스 휠` | Zoom |
| `F1` | Frustum Culling ON / OFF |
| `F2` | QuadTree LOD ON / OFF |
| `1` | Cube 표시 |
| `2` | Perlin Terrain 표시 |
| `3` | HeightMap Terrain 표시 |
| `4` | 기존 QuadTree LOD Terrain 표시 |
| `5` | Triplanar Terrain 표시 |
| `6 ~ 0` | 예약 |

## 정상 동작 기준

```text
1. 실행 직후 Triplanar Terrain만 보인다.
2. 숫자키 5로 Triplanar Terrain을 숨기고 다시 켤 수 있다.
3. 숫자키 4로 기존 UV 기반 QuadTree LOD Terrain을 비교할 수 있다.
4. 가파른 절벽에서 Triplanar Texture가 덜 늘어나 보인다.
5. Grass / Rock / Snow Height/Slope 규칙은 유지된다.
6. F1 Culling이 계속 정상 동작한다.
7. F2 LOD가 계속 정상 동작한다.
8. Camera 거리 변화에 따라 Surface Triangle 통계가 변한다.
9. 기존 Feature 파일은 그대로 남아있다.
```

## 인코딩

```text
.cpp / .h
→ UTF-8 BOM

.hlsl
→ UTF-8 without BOM
```
