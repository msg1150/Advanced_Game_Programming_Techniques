# Height / Slope 기반 Texture Splatting Terrain

HeightMap Terrain의 **정점 높이와 Normal에서 얻은 경사도**를 이용해 Grass / Rock / Snow Texture를 자동으로 혼합하는 단계입니다.

이전의 Splat Map RGB 방식은 제거하고, Terrain Geometry 자체의 정보를 이용해 각 Pixel의 Texture Weight를 자동 계산하도록 수정했습니다.

## 핵심 구현

- Height 기반 Texture Weight 계산
- Vertex Normal 기반 Slope 계산
- 낮고 완만한 지형 → Grass
- 가파른 지형 → Rock
- 높고 완만한 지형 → Snow
- `smoothstep` 기반 자연스러운 Texture Blend
- Pixel 단위 Weight 계산
- 기존 HeightMap Geometry / Normal 재사용
- 공용 `Texture2D` 재사용
- Texture Tiling 및 Mip Map
- 기존 Framework / Terrain 코드 영향 최소화

## 최종 규칙

```text
낮고 완만한 곳
→ Grass

경사가 큰 곳
→ Rock

높고 완만한 곳
→ Snow
```

Texture를 정점에서 하나로 고정하지 않고, Vertex Shader에서 전달된 Height / Normal이 Rasterizer에서 보간된 뒤 **Pixel Shader에서 매 Pixel마다 Weight를 계산**합니다.

따라서 Layer 경계가 Triangle 단위로 끊기지 않고 부드럽게 이어집니다.

## 구조

```text
Source/
├─ Graphics/
│  └─ Texture2D
│
└─ Features/
   ├─ HeightMapTerrain/
   │  └─ 기존 Geometry 생성 코드
   │
   └─ TextureSplatting/
      ├─ TextureSplatMaterial.h
      ├─ TextureSplatMaterial.cpp
      ├─ TextureSplatTerrain.h
      └─ TextureSplatTerrain.cpp

Shaders/
└─ TextureSplatting/
   ├─ TextureSplatVS.hlsl
   └─ TextureSplatPS.hlsl

Assets/
└─ Textures/
   └─ Terrain/
      ├─ Grass.png
      ├─ Rock.png
      └─ Snow.png
```

별도의 `SplatMap.png`는 사용하지 않습니다.

## 전체 처리 흐름

```text
HeightMap
   ↓
HeightMapTerrainGenerator
   ↓
Position / Normal / UV
   ↓
TextureSplatVS
   │
   ├─ World Position
   ├─ World Normal
   └─ UV
   ↓
Rasterizer 보간
   ↓
TextureSplatPS
   │
   ├─ Height 계산
   ├─ Slope 계산
   ├─ Grass Weight
   ├─ Rock Weight
   └─ Snow Weight
   ↓
Weight 정규화
   ↓
Grass / Rock / Snow Blend
```

## Height 계산

Vertex의 World Position Y를 사용합니다.

```hlsl
float height =
    input.WorldPosition.y;
```

Terrain을 다른 Y 위치로 이동하면 World Height도 함께 변합니다.

## Slope 계산

기존 HeightMap Terrain Generator가 계산한 Smooth Vertex Normal을 재사용합니다.

```hlsl
float3 normal =
    normalize(input.WorldNormal);

float upDot =
    saturate(
        dot(
            normal,
            float3(0, 1, 0)));

float slope =
    1.0 - upDot;
```

의미:

```text
Slope ≈ 0
→ 평평한 지형

Slope ≈ 1
→ 수직에 가까운 지형
```

## Grass Weight

Grass는 낮고 완만한 영역을 기준으로 계산합니다.

```text
Height 낮음
+
Slope 낮음
↓
Grass 증가
```

높이가 올라가거나 경사가 커지면 Grass Weight가 감소합니다.

## Rock Weight

Rock은 경사도를 가장 강하게 사용합니다.

```text
RockSlopeStart
        ↓
Rock 증가 시작

RockSlopeEnd
        ↓
Rock 지배 영역
```

따라서 높은 산이라도 절벽처럼 가파르면 Snow보다 Rock이 우선합니다.

## Snow Weight

Snow는 Height를 기준으로 증가하지만, Rock이 강한 절벽에서는 감소합니다.

```text
Height 높음
+
Slope 완만
↓
Snow 증가
```

즉 정상적으로 동작하면 산 정상의 평평한 부분은 Snow가 되고, 같은 높이의 절벽 면은 Rock으로 표시됩니다.

## smoothstep Blend

각 Texture를 단순 조건문으로 전환하지 않습니다.

```text
if Height > X
→ Snow
```

같은 방식은 Texture 경계를 딱 잘라 보이게 만듭니다.

현재 구현은:

```hlsl
smoothstep(Start, End, Value)
```

를 사용해 일정 범위 동안 Weight가 점진적으로 변하도록 합니다.

```text
Grass ───── Blend ───── Rock
Rock  ───── Blend ───── Snow
```

## 설정값

실제 기준값은 `Source/Core/Application.cpp`에서 조절합니다.

```cpp
splatMaterialDesc.TextureTiling =
    12.0f;

splatMaterialDesc.GrassFadeStartHeight =
    0.55f;

splatMaterialDesc.GrassFadeEndHeight =
    1.35f;

splatMaterialDesc.RockSlopeStart =
    0.18f;

splatMaterialDesc.RockSlopeEnd =
    0.48f;

splatMaterialDesc.SnowStartHeight =
    1.25f;

splatMaterialDesc.SnowFullHeight =
    2.35f;
```

### 설정값 의미

| 설정 | 역할 |
|---|---|
| `TextureTiling` | Texture 반복 횟수 |
| `GrassFadeStartHeight` | Grass 감소 시작 높이 |
| `GrassFadeEndHeight` | Grass가 크게 줄어드는 높이 |
| `RockSlopeStart` | Rock 증가 시작 경사 |
| `RockSlopeEnd` | Rock이 강해지는 경사 |
| `SnowStartHeight` | Snow 증가 시작 높이 |
| `SnowFullHeight` | Snow가 최대에 가까워지는 높이 |

## Texture Slot

```text
t0 → Grass
t1 → Rock
t2 → Snow
```

이전 버전의:

```text
t3 → SplatMap
```

은 제거했습니다.

## Texture Tiling

Terrain UV는 전체 기준 `0 ~ 1`이지만, Texture는 다음과 같이 반복합니다.

```text
TiledUV =
TerrainUV × TextureTiling
```

기본값 `12.0`이면 Terrain 전체에서 Texture가 약 12회 반복됩니다.

Sampler Address Mode는 `WRAP`을 사용합니다.

## 기존 HeightMap Geometry 재사용

이번 단계에서 새로운 Terrain Mesh 생성 코드를 만들지 않습니다.

기존:

```text
HeightMapImage
HeightMapTerrainGenerator
```

를 그대로 사용합니다.

특히 `HeightMapTerrainGenerator`가 계산한 Smooth Vertex Normal이 자동 Splatting의 Slope 입력으로 바로 사용됩니다.

## 기존 코드 영향 최소화

### 새로 추가된 공용 기능

```text
Graphics/Texture2D
```

### Texture Splatting Feature

```text
Features/TextureSplatting
Shaders/TextureSplatting
Assets/Textures/Terrain
```

### 기존 구현 수정 없음

```text
Renderer
Mesh
Shader
Camera
CameraController
Input
Window

PerlinTerrain
HeightMapTerrain
HeightMapTerrainGenerator
HeightMapImage
```

Application에서는 Feature 초기화 설정과 `Render()` 호출만 연결합니다.

## Color 처리

Grass / Rock / Snow는 SRGB Texture로 로드됩니다.

현재 Framework BackBuffer는 SRGB RenderTarget이 아닌 `UNORM`이므로, 이번 Shader에서는 최종 Linear Color를 화면 표시용으로 근사 Gamma Encode합니다.

이 처리는 이후 Framework 자체에 SRGB BackBuffer를 추가하면 Renderer 단계로 이전할 수 있습니다.

## 현재 단계에서 제외

```text
Normal Mapping
Directional Lighting
PBR
4개 이상 Layer
Biome
Runtime Texture Painting
Manual Splat Map 보정
```

현재 목표는 **Height / Slope에 따른 자동 Texture 분배와 Blend 검증**입니다.

## 실행

```text
1. DXFramework.sln 실행
2. Debug | x64
3. F5
```

기존 Camera 조작:

| 입력 | 기능 |
|---|---|
| `W / A / S / D` | 이동 |
| `좌클릭 + Drag` | Orbit |
| `마우스 휠` | Zoom |

## 정상 출력 기준

화면 중앙의 Solid Terrain에서:

```text
낮은 평지
→ Grass

산의 가파른 측면
→ Rock

높은 완만한 정상
→ Snow
```

가 Terrain 형태에 맞게 나타나면 정상입니다.

## 인코딩

```text
.cpp / .h
→ UTF-8 BOM

.hlsl
→ UTF-8 without BOM
```
