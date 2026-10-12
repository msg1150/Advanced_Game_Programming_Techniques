# DirectX 11 Terrain Streaming v0.15 — Collision Framework + GPU Picking

기존 **v0.14 UI Designer v0.2 / Disk Terrain Streaming** 프로젝트를 유지한 상태에서, 독립적인 Collision Framework와 DirectX 11 GPU Picking, 그리고 기존 Custom UI를 이용한 Collider 배치/편집 기능을 추가한 버전입니다.

이번 단계의 핵심은 `하나의 Collision으로 모든 물체를 처리`하는 방식이 아니라, **Collider 공통 정보 / Shape / Broad Phase / Narrow Phase / Query / Rendering Feature를 분리**해서 Shape가 늘어나도 기존 시스템을 크게 수정하지 않도록 만드는 것입니다.

> 이 버전은 **Collision Detection + Scene Query + Editor-style Collider 배치/선택**까지 구현합니다.  
> Rigidbody, 중력, 속도, 마찰, 반발, 충돌 후 밀어내기 같은 **Physics Solver / Collision Response는 아직 구현하지 않습니다.**

---

## 1. 이번 버전에서 추가된 기능

### Collision Core

지원 Shape:

- `Sphere`
- `AABB`
- `OBB`
- `Capsule`

각 Collider는 다음 공통 정보를 가집니다.

- 고유 `ColliderId`
- 이름
- Position
- Rotation
- Shape Data
- Layer
- Mask
- Enabled
- Trigger 여부

`ColliderId`는 삭제 후에도 재사용하지 않습니다. GPU Picking 결과가 몇 Frame 늦게 CPU에 도착하더라도 삭제된 ID가 새 Collider를 잘못 선택하는 문제를 막기 위한 구조입니다.

### Broad Phase

모든 Shape는 `ComputeWorldBounds()`를 통해 공통 World AABB를 제공합니다.

현재 Broad Phase는 **X축 Sweep-and-Prune** 방식입니다.

```text
전체 Collider
    ↓
World AABB 생성
    ↓
Min X 기준 정렬
    ↓
Layer / Mask + AABB 후보 제거
    ↓
Narrow Phase 후보 Pair
```

기존 Terrain LOD용 QuadTree에 Collision 책임을 억지로 추가하지 않았습니다. Collision Spatial 구조는 Physics 쪽에서 독립적으로 확장할 수 있습니다.

### Narrow Phase

현재 4개 Shape의 조합을 처리합니다.

- Sphere ↔ Sphere
- Sphere ↔ AABB
- Sphere ↔ OBB
- Sphere ↔ Capsule
- AABB ↔ AABB
- AABB ↔ OBB
- AABB ↔ Capsule
- OBB ↔ OBB
- OBB ↔ Capsule
- Capsule ↔ Capsule

반대 순서 Pair도 Dispatcher에서 처리합니다.

충돌 결과는 단순 `bool`만 반환하지 않고 다음 정보를 보관합니다.

```text
Hit
Collider A / B
Contact Point
Contact Normal
Penetration Depth
Trigger Pair 여부
```

OBB 계열 판정에는 SAT 축 검사를 사용합니다.

### Layer / Mask

Collider마다 `Layer`와 `Mask`를 분리했습니다.

Broad Phase에서 양쪽 Mask가 상대 Layer를 허용하지 않으면 Narrow Phase에 전달하지 않습니다.

기본 Layer 상수:

- `LayerDefault`
- `LayerTerrain`
- `LayerTrigger`
- `LayerEditor`
- `LayerAll`

현재 Runtime UI에서는 Layer/Mask Bit 편집 화면까지 노출하지 않았지만 Physics Core에는 이미 동작하는 구조가 포함되어 있습니다.

### Trigger

`IsTrigger = true`인 Collider도 Collision Detection은 수행합니다.

다만 이번 버전에는 Physics Response가 없으므로 Trigger는 **접촉 정보를 구분하는 용도**입니다.

```text
Collision Detection : O
Contact 정보        : O
물리 반응           : X
```

---

## 2. Scene Query

GPU Picking과 별개로 CPU Query도 분리했습니다.

### Raycast

`Physics/Query/Raycast`에서 다음 Shape를 Raycast할 수 있습니다.

- Sphere
- AABB
- OBB
- Capsule

가장 가까운 Collider를 반환하는 `Raycast::Closest()`도 포함합니다.

### Overlap

`CollisionWorld::OverlapAABB()`를 통해 지정한 공간과 겹치는 Collider ID들을 조회할 수 있습니다.

GPU Picking은 **화면에서 사용자가 무엇을 클릭했는지** 찾기 위한 Graphics Feature이고, CPU Raycast/Overlap은 Gameplay·AI·Debug 등에서 사용할 Scene Query이므로 서로 종속시키지 않았습니다.

---

## 3. DirectX 11 GPU Picking

GPU Picking은 Collision System 내부가 아니라 별도 Feature로 구현했습니다.

```text
Source/Features/GPUPicking/
└─ GPUPickingSystem.*
```

Picking Pass에서 사용하는 Target:

```text
MRT 0 : DXGI_FORMAT_R32_UINT
        → Collider Object ID

MRT 1 : DXGI_FORMAT_R32_FLOAT
        → Screen Depth
```

### 처리 흐름

```text
Scene Left Click
    ↓
Custom UI가 Mouse를 사용했는지 확인
    ↓
GPU Picking 요청
    ↓
Collision Shape Solid Proxy 렌더
    ↓
기존 Terrain Scene Depth로 Depth Test
    ↓
Mouse 위치 1x1 Pixel만 Staging으로 복사
    ↓
Event Query Ring으로 GPU 완료 확인
    ↓
CPU Stall 없이 ID / Depth Readback
    ↓
Collider 선택
```

### Scene Depth 재사용

Terrain이 먼저 기록한 기존 DepthStencilView를 Picking Pass에서 재사용합니다.

Picking용 Depth State는 다음과 같습니다.

- Depth Test: ON
- Depth Write: OFF
- Compare: `LESS_EQUAL`

따라서 Terrain 뒤에 완전히 가려진 Collider Proxy가 마우스로 선택되는 문제를 줄입니다.

### 1x1 Scissor

Picking Pass의 Rasterizer에는 Scissor를 켜고 마우스 위치 `1 x 1 Pixel`만 통과시킵니다.

즉 Vertex/Primitive 처리는 수행하지만 화면 전체에 Pixel Shader 결과를 쓸 필요는 없습니다.

### 1x1 Readback

1920×1080 같은 전체 Picking Texture를 CPU로 읽지 않습니다.

`CopySubresourceRegion()`으로 클릭 Pixel 하나만 다음 Staging Texture에 복사합니다.

- `1x1 R32_UINT Staging`
- `1x1 R32_FLOAT Staging`

### 비동기 Readback Ring

Readback Slot을 3개 사용합니다.

각 Slot은 다음을 가집니다.

- ID Staging Texture
- Depth Staging Texture
- `D3D11_QUERY_EVENT`
- 클릭 당시 Mouse X/Y
- 클릭 당시 Inverse ViewProjection
- Sequence

`GetData(... D3D11_ASYNC_GETDATA_DONOTFLUSH)`로 완료 여부를 확인한 뒤 준비된 Slot만 `Map`합니다.

GPU가 아직 끝나지 않았으면 CPU가 기다리지 않고 다음 Frame으로 넘어갑니다.

### World Position 복원

Picking에서 얻은:

- Mouse X/Y
- Screen Depth
- Inverse ViewProjection

을 이용해 선택된 지점의 World Position도 복원합니다.

Custom UI의 GPU Pick 상태에서 Depth와 World Position을 확인할 수 있습니다.

> 현재 GPU Picking의 ID 대상은 **Collider Proxy**입니다. Terrain은 Picking 대상 ID로 등록하지 않고, Collider 가림 판정을 위한 Scene Depth로만 사용합니다.

---

## 4. 기존 Custom UI를 이용한 Collider 설치 / 편집

새로운 별도 GUI 라이브러리를 추가하지 않았습니다.

기존 프로젝트에서 직접 만든:

- `UIManager`
- `UIPanel`
- `UILabel`
- `UIButton`
- `UICheckBox`
- `UISlider`

를 그대로 사용합니다.

실행 후 상단의 **`Collision 열기`** 버튼으로 Panel을 열 수 있습니다.

### Collider 추가

Panel에서 다음 버튼을 누르면 현재 Camera Target 주변의 3D 공간에 Collider가 실제로 추가됩니다.

- `Sphere 추가`
- `AABB 추가`
- `OBB 추가`
- `Capsule 추가`

동일 위치에 전부 겹쳐 생성하지 않도록 Camera Target 주변에 간격을 두어 배치합니다.

새로 추가한 Collider는 자동으로 선택됩니다.

### GPU Picking으로 선택

Custom UI가 아닌 3D Scene에서 Collider 내부를 Left Click하면 GPU ID Picking을 통해 Collider를 선택합니다.

UI 위 클릭은 `UIManager`가 소비하므로 Picking과 Camera Orbit으로 전달하지 않습니다.

### 선택 Collider 편집

Inspector에서 다음 값을 Runtime 중 직접 수정할 수 있습니다.

```text
Enabled
Trigger

Position X / Y / Z
Rotation X / Y / Z

Radius
Capsule Half Segment

Half Extent X / Y / Z
```

Shape별 실제 사용 값:

| Shape | Position | Rotation | Radius | Half Segment | Half Extent |
|---|---:|---:|---:|---:|---:|
| Sphere | O | 무시 | O | 무시 | 무시 |
| AABB | O | 무시 | 무시 | 무시 | O |
| OBB | O | O | 무시 | 무시 | O |
| Capsule | O | O | O | O | 무시 |

UI에 현재 선택 Shape가 어떤 Parameter를 사용하는지도 표시합니다.

### 삭제

- 선택 해제
- 선택 Collider 삭제
- 전체 Collider 삭제

를 UI에서 수행할 수 있습니다.

---

## 5. Collision Debug Rendering

Collider는 실제 Game Mesh와 독립된 Wireframe으로 표시합니다.

색상 의미:

```text
일반 Collider  : Cyan 계열
Trigger         : Purple 계열
충돌 중         : Red 계열
현재 선택       : Yellow 계열
Contact Debug   : Green 계열
```

UI에서 다음을 켜거나 끌 수 있습니다.

- Collider Debug Wireframe
- 충돌 중 Collider 강조
- Contact Point / Normal 표시
- GPU Picking

Collision Debug는 Scene Depth를 사용하지만 Depth Write는 하지 않습니다.

---

## 6. 코드 구조

```text
Source/
├─ Physics/
│  ├─ Collision/
│  │  ├─ Core/
│  │  │  ├─ CollisionMath.h
│  │  │  ├─ CollisionTypes.h
│  │  │  ├─ CollisionResult.h
│  │  │  └─ CollisionWorld.*
│  │  ├─ BroadPhase/
│  │  │  └─ BroadPhaseSystem.*
│  │  └─ Detection/
│  │     └─ CollisionDetector.*
│  └─ Query/
│     └─ Raycast.*
│
└─ Features/
   ├─ CollisionEditor/
   │  ├─ CollisionGeometry.*
   │  ├─ CollisionDebugRenderer.*
   │  ├─ CollisionEditorPanel.*
   │  └─ CollisionScene.*
   └─ GPUPicking/
      └─ GPUPickingSystem.*

Shaders/
├─ CollisionDebug/
│  ├─ CollisionDebugVS.hlsl
│  └─ CollisionDebugPS.hlsl
└─ GPUPicking/
   ├─ PickingVS.hlsl
   └─ PickingPS.hlsl
```

### 의존 방향

```text
Physics Core
    ↑
    ├─ CPU Query
    └─ CollisionEditor Geometry Adapter
             ↑
       Debug / GPU Picking
             ↑
       CollisionScene Facade
             ↑
          Application
```

Physics Core는 Win32 / D3D11 / DirectXMath에 의존하지 않습니다.

따라서 나중에 Collision Debug 또는 GPU Picking Feature를 제거하더라도 Narrow Phase와 Query 코드를 그대로 사용할 수 있습니다.

---

## 7. 기존 Terrain / UI 기능

v0.14까지 구현했던 기능은 유지합니다.

- Disk Terrain Streaming
- QuadTree LOD
- Async Geomorphing
- Geomorph Debug
- Predictive Prefetch
- CPU Tile Sample Cache
- Streaming / Cache Metrics
- Streaming MiniMap
- Custom Direct2D/DirectWrite UI Framework
- Scrollable Panel
- Numeric Input
- UIDocument XML
- 외부 Win32 UI Designer
- UI Designer Runtime Canvas Preview

Collision용으로 기존 Terrain LOD QuadTree를 수정하거나 Collision 책임을 추가하지 않았습니다.

---

## 8. 테스트

`Tests/`에는 DirectX/Win32 없이 빌드할 수 있는 Portable Test를 추가했습니다.

```text
Tests/
├─ test_collision_shapes.cpp
├─ test_collision_world.cpp
└─ test_raycast.cpp
```

확인 항목:

- Shape Narrow Phase 기본 충돌/비충돌
- OBB Rotation
- Capsule Pair
- Broad Phase
- Layer / Mask
- Trigger Pair
- Collider ID 재사용 방지
- Overlap AABB
- Sphere / AABB / OBB / Capsule Raycast
- Raycast Layer filtering

제작 환경에서는 위 Physics Core 테스트를 `g++ -std=c++20 -Wall -Wextra -Wpedantic`로 실행해 통과한 상태입니다.

### Windows 확인 필요 항목

이 작업 환경에는 Windows SDK / Visual Studio / 실제 DirectX 11 GPU 실행 환경이 없기 때문에 다음 항목은 여기서 직접 실행 검증할 수 없습니다.

- Visual Studio 2022 `Debug | x64` 전체 Build
- HLSL의 실제 `D3DCompile`
- GPU Picking RenderTarget 생성/실행
- D3D11 Event Query / Staging Readback 실기기 동작
- Custom UI와 Scene Click 실제 입력 동작

따라서 프로젝트를 받은 뒤 **Visual Studio 2022에서 Debug x64 Build 및 Runtime 확인이 마지막 단계**입니다.

---

## 9. 현재 의도적으로 제외한 부분

이번 버전에는 다음을 넣지 않았습니다.

- Rigidbody
- Gravity
- Velocity / Acceleration
- Mass
- Friction
- Restitution
- Impulse Solver
- 충돌 후 자동 위치 보정 / 밀어내기
- Convex Hull Collider
- Triangle Mesh Collider
- HeightField / Terrain Collider
- Ellipsoid Collider
- Compound Collider Editor

현재 구조는 이 기능들을 이후 별도 Module로 확장하기 위한 기반입니다.

특히 다음 단계에서 Physics Response가 필요해지더라도 기존 `CollisionDetector`가 직접 물체를 움직이게 하지 않고:

```text
Collision Detection
       ↓
CollisionResult
       ↓
별도 Physics Response / Solver
```

구조를 유지하는 방향을 권장합니다.

---

## 10. 실행 순서

1. `DXFramework.sln`을 Visual Studio 2022로 엽니다.
2. `Debug | x64`로 Build합니다.
3. 실행 후 Terrain이 정상 표시되는지 확인합니다.
4. 상단 `Collision 열기`를 누릅니다.
5. `Sphere / AABB / OBB / Capsule 추가` 중 하나를 누릅니다.
6. 3D Scene에 표시되는 Wireframe Collider를 확인합니다.
7. Collider를 겹치게 이동해 Red Collision Highlight와 Contact를 확인합니다.
8. Scene의 Collider를 Left Click해 GPU Picking 선택이 되는지 확인합니다.
9. Position / Rotation / Size Slider와 숫자 입력을 변경해 실시간 반영을 확인합니다.
10. Terrain 뒤에 가려진 Collider가 클릭되지 않는지도 확인합니다.

