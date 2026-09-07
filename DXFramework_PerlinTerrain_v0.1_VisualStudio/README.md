# Perlin Noise Terrain

기존 DirectX Framework 위에 **Seed 기반 Perlin Noise Terrain**을 독립 Feature로 추가한 단계입니다.

가장 중요한 목표는 Terrain 기능을 위해 기존 `Renderer`, `Mesh`, `Shader`, `Camera` 구조를 수정하지 않고, 나중에 Perlin Terrain을 제거하거나 다른 Terrain 방식이 추가되어도 영향이 적도록 만드는 것입니다.

## 핵심 구현

- Seed 기반 2D Improved Perlin Noise
- Perlin Noise 기반 Grid Height 생성
- Vertex / Index 생성
- UV 생성
- Smooth Vertex Normal 계산
- Terrain 전용 Shader
- Terrain 전용 Wireframe Rasterizer State
- 기존 Framework의 `Mesh / Shader / ConstantBuffer` 재사용
- Perlin Terrain과 Framework 코드 분리

## 구조

```text
Source/
└─ Features/
   └─ PerlinTerrain/
      ├─ PerlinNoise.h
      ├─ PerlinNoise.cpp
      ├─ PerlinTerrainGenerator.h
      ├─ PerlinTerrainGenerator.cpp
      ├─ PerlinTerrain.h
      └─ PerlinTerrain.cpp

Shaders/
└─ PerlinTerrain/
   ├─ TerrainVS.hlsl
   └─ TerrainPS.hlsl
```

## 처리 흐름

```text
PerlinNoise
    ↓
Noise(x, z)
    ↓
TerrainGenerator
    ↓
Vertex Y 계산
    ↓
Vertex / Index / UV / Normal
    ↓
Mesh
    ↓
PerlinTerrain::Render()
```

## PerlinNoise

`PerlinNoise`는 Terrain이나 DirectX에 의존하지 않는 순수 수학 클래스입니다.

```text
Seed
 ↓
Permutation Table
 ↓
Gradient
 ↓
Dot Product
 ↓
Fade
 ↓
Lerp
 ↓
Noise Value
```

같은 Seed와 같은 좌표를 사용하면 항상 동일한 결과를 반환합니다.

```text
Seed 12345
→ 항상 동일한 Terrain

Seed 변경
→ 다른 Terrain
```

Fade 함수:

```text
6t^5 - 15t^4 + 10t^3
```

## Terrain 생성

Grid의 각 Vertex 위치를 기준으로 Noise를 Sampling합니다.

```text
Y =
PerlinNoise(
    X × NoiseFrequency,
    Z × NoiseFrequency
)
× HeightScale
```

Vertex 배열 번호가 아니라 공간 좌표를 기반으로 Sampling하기 때문에 이후 Chunk Terrain으로 확장하기에도 유리한 구조입니다.

## Terrain 설정

실제 실행 시 사용할 값은 `Application.cpp`에서 지정합니다.

현재 설정:

```cpp
PerlinTerrainSettings terrainSettings = {};

terrainSettings.CellsX = 64;
terrainSettings.CellsZ = 64;
terrainSettings.CellSize = 0.5f;
terrainSettings.NoiseFrequency = 0.08f;
terrainSettings.HeightScale = 3.0f;
terrainSettings.Seed = 12345;
```

### 설정값 의미

| 값 | 역할 |
|---|---|
| `CellsX` | X 방향 Grid Cell 개수 |
| `CellsZ` | Z 방향 Grid Cell 개수 |
| `CellSize` | Vertex 간 World 간격 |
| `NoiseFrequency` | Noise 굴곡 빈도 |
| `HeightScale` | 높이 배율 |
| `Seed` | Terrain 패턴 결정 |

## Normal 계산

Perlin Noise 적용 후 Terrain의 실제 기울기를 기준으로 Triangle Face Normal을 구하고 각 Vertex에 누적한 뒤 Normalize합니다.

```text
Triangle Edge
   ↓
Cross Product
   ↓
Face Normal
   ↓
Vertex에 누적
   ↓
Normalize
```

현재 단계에서는 Lighting을 사용하지 않지만 이후 조명 구현에서 바로 사용할 수 있도록 Normal을 미리 생성합니다.

## Wireframe

기존 `Renderer`에 Terrain 전용 Wireframe 기능을 추가하지 않았습니다.

`PerlinTerrain`이 자신의 Rasterizer State를 가지고 있습니다.

```text
기존 Rasterizer State 저장
        ↓
D3D11_FILL_WIREFRAME
        ↓
Terrain Draw
        ↓
기존 State 복구
```

따라서 다른 Mesh의 렌더링 상태에 영향을 남기지 않습니다.

## Framework와의 연결

기존 Framework의 실제 Graphics 구현은 수정하지 않습니다.

Perlin Terrain은 주로 `Application`에서 다음 정도만 연결됩니다.

```text
PerlinTerrain 멤버
Initialize()
Render()
```

이 구조 덕분에 Perlin Terrain을 제거할 경우 Feature 폴더와 Application 연결부만 제거하면 됩니다.

## 현재 제외한 기능

기본 Perlin 구현 자체를 먼저 검증하기 위해 다음 기능은 아직 포함하지 않습니다.

```text
FBM / Octave
Persistence
Lacunarity
Texture
Lighting
Chunk
LOD
Erosion
```

## 실행

```text
1. DXFramework.sln 실행
2. Debug | x64
3. F5
```

기존 Camera 조작은 Framework와 동일합니다.

| 입력 | 기능 |
|---|---|
| `W / A / S / D` | 이동 |
| `좌클릭 + Drag` | Orbit |
| `마우스 휠` | Zoom |

## 인코딩

```text
.cpp / .h
→ UTF-8 BOM

.hlsl
→ UTF-8 without BOM
```
