# HeightMap Image Terrain

이미지의 픽셀 밝기를 높이값으로 변환하여 Terrain을 생성하는 **HeightMap 기반 Terrain Feature**입니다.

Perlin Terrain과 동일하게 기존 Framework 내부 구현을 수정하지 않고 독립 Feature로 추가했으며, HeightMap 기능을 제거하더라도 기존 Framework와 Perlin Terrain에 영향을 주지 않는 구조를 목표로 합니다.

## 핵심 구현

- Windows WIC 기반 이미지 로딩
- RGB 픽셀 → 밝기(Luminance) 계산
- 밝기값 → Terrain 높이 변환
- 이미지 해상도 기반 Grid 생성
- Vertex / Index / UV 생성
- Smooth Vertex Normal 계산
- HeightMap Terrain 전용 Shader
- Feature 내부 Wireframe Rasterizer State
- 테스트용 HeightMap PNG 포함

## 구조

```text
Source/
└─ Features/
   └─ HeightMapTerrain/
      ├─ HeightMapImage.h
      ├─ HeightMapImage.cpp
      ├─ HeightMapTerrainGenerator.h
      ├─ HeightMapTerrainGenerator.cpp
      ├─ HeightMapTerrain.h
      └─ HeightMapTerrain.cpp

Shaders/
└─ HeightMapTerrain/
   ├─ TerrainVS.hlsl
   └─ TerrainPS.hlsl

Assets/
└─ HeightMaps/
   └─ HeightMap_Test.png
```

## 전체 처리 흐름

```text
HeightMap_Test.png
        ↓
HeightMapImage
        ↓
RGBA Pixel
        ↓
Luminance
        ↓
0.0 ~ 1.0
        ↓
MinHeight ~ MaxHeight
        ↓
HeightMapTerrainGenerator
        ↓
Vertex / Index / UV / Normal
        ↓
Mesh
        ↓
HeightMapTerrain::Render()
```

## 이미지 로딩

외부 이미지 라이브러리를 추가하지 않고 Windows 기본 기능인 **WIC(Windows Imaging Component)**를 사용합니다.

사용 라이브러리:

```text
ole32.lib
windowscodecs.lib
```

HeightMap 용도로는 PNG 사용을 권장합니다.

## 픽셀 밝기 → 높이

RGB 값을 사람이 인식하는 밝기에 가까운 Luminance 값으로 변환합니다.

```cpp
luminance =
    0.2126f * R +
    0.7152f * G +
    0.0722f * B;
```

그 후:

```text
0 ~ 255
   ↓
0.0 ~ 1.0
```

범위로 정규화합니다.

실제 Terrain 높이는 다음 방식으로 계산합니다.

```text
Height =
MinHeight +
NormalizedPixel × (MaxHeight - MinHeight)
```

따라서 기본적으로:

```text
검정
→ 가장 낮은 지형

회색
→ 중간 높이

흰색
→ 가장 높은 지형
```

이 됩니다.

## Terrain 설정

실제 실행에 사용할 HeightMap과 높이 범위는 `Application.cpp`에서 설정합니다.

```cpp
HeightMapTerrainSettings heightMapTerrainSettings = {};

heightMapTerrainSettings.CellSize = 0.05f;
heightMapTerrainSettings.MinHeight = -1.0f;
heightMapTerrainSettings.MaxHeight = 3.0f;

const auto heightMapImagePath =
    assetRoot /
    L"HeightMaps" /
    L"HeightMap_Test.png";
```

### 설정값 의미

| 값 | 역할 |
|---|---|
| `CellSize` | 인접 Vertex 사이 World 간격 |
| `MinHeight` | 검정색이 대응할 최소 높이 |
| `MaxHeight` | 흰색이 대응할 최대 높이 |
| `heightMapImagePath` | 사용할 HeightMap 이미지 |

## 이미지 해상도와 Terrain 해상도

현재 구현은 **픽셀 하나를 Vertex 하나로 사용**합니다.

예:

```text
129 × 129 HeightMap
        ↓
129 × 129 Vertex
        ↓
128 × 128 Grid Cell
```

따라서 이미지 해상도가 높아질수록 Terrain Mesh도 촘촘해집니다.

## 좌표 매핑

현재 X축은 이미지 방향을 그대로 사용합니다.

```text
이미지 왼쪽
→ World -X

이미지 오른쪽
→ World +X
```

코드에서 X축 좌우 반전은 하지 않습니다.

이미지의 세로 픽셀 좌표는 Terrain의 Z축으로 사용합니다.

2D 이미지와 3D Camera 시점은 축 방향이 다르기 때문에 보는 방향에 따라 이미지와 좌우/상하 감각이 다르게 느껴질 수 있습니다.

## Normal / UV

Height 적용 후 실제 Triangle을 기준으로 Smooth Vertex Normal을 계산합니다.

UV는 HeightMap 전체 기준으로 `0 ~ 1` 범위를 생성합니다.

```text
(0,0) -------- (1,0)
  |                |
  |                |
(0,1) -------- (1,1)
```

현재 단계에서는 Texture를 사용하지 않지만 이후 Terrain Texture 기능에서 그대로 사용할 수 있습니다.

## Wireframe

HeightMap Terrain도 기존 Renderer를 변경하지 않습니다.

```text
기존 Rasterizer State 저장
        ↓
HeightMap Wireframe State
        ↓
Terrain Draw
        ↓
기존 State 복구
```

따라서 Perlin Terrain, Cube 등 다른 Render Object와 함께 사용할 수 있습니다.

## 테스트 이미지

`Assets/HeightMaps/HeightMap_Test.png`

테스트 이미지에는 Height 변화가 눈으로 확인되도록 다음 패턴이 포함되어 있습니다.

```text
중앙의 높은 영역
사선 형태의 Ridge
낮은 영역
완만한 굴곡
```

이미지의 밝은 부분이 높고 어두운 부분이 낮게 표시되면 정상입니다.

## Feature 독립성

HeightMap Terrain은 다음 기존 구현을 수정하지 않습니다.

```text
Renderer
Mesh
Shader
Camera
CameraController
Input
Window
Transform

PerlinNoise
PerlinTerrainGenerator
PerlinTerrain
```

Application에서는 Feature 객체의 `Initialize()`와 `Render()`만 연결합니다.

HeightMap Terrain을 제거해야 할 경우 `HeightMapTerrain` Feature 폴더, Shader, Asset과 Application 연결부만 제거하면 됩니다.

## 실행

```text
1. DXFramework.sln 실행
2. Debug | x64
3. F5
```

기존 Camera 조작을 그대로 사용합니다.

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
