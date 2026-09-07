# DirectX Framework

DirectX 11 기반 실습 및 기능 확장을 위한 공용 프레임워크입니다.

이 단계의 핵심 목표는 이후 Terrain, Texture, Material, Lighting 등의 기능을 추가하더라도 기존 코드를 크게 수정하지 않도록 **Window / Input / Camera / Renderer / Mesh / Shader를 역할별로 분리하는 것**입니다.

## 핵심 구현

- Direct3D 11 Device / DeviceContext / SwapChain 초기화
- RenderTarget / DepthStencil / Viewport 관리
- 공용 `Mesh` 클래스 기반 Vertex / Index Buffer 생성 및 렌더링
- HLSL Vertex / Pixel Shader 런타임 컴파일
- Transform Constant Buffer 지원
- Perspective Orbit Camera 구현
- 키보드 / 마우스 입력 시스템 분리
- 초기화 실패 단계 및 Shader 컴파일 오류 출력
- 테스트 Cube를 이용한 렌더링 검증

## 구조

```text
Source/
├─ Core/
│  ├─ Application
│  └─ Timer
├─ Graphics/
│  ├─ Renderer
│  ├─ Camera
│  ├─ CameraController
│  ├─ Mesh
│  ├─ Shader
│  ├─ Transform
│  └─ ConstantBuffer
├─ Input/
│  └─ Input
├─ Platform/
│  └─ Window
└─ Main.cpp

Shaders/
└─ Basic/
   ├─ BasicVS.hlsl
   └─ BasicPS.hlsl
```

## 설계 포인트

### 1. 기능별 책임 분리

`Application`은 전체 초기화 및 실행 흐름만 관리하고, 실제 기능은 각 전용 클래스에 맡깁니다.

```text
Application
├─ Window
├─ Input
├─ Timer
├─ Renderer
├─ Camera
├─ CameraController
├─ Mesh
└─ Shader
```

특정 기능이 `Application`에 직접 종속되지 않도록 하여 이후 기능 추가 시 영향을 최소화합니다.

### 2. Camera와 Input 분리

Camera가 키보드나 마우스를 직접 읽지 않습니다.

```text
Input
  ↓
CameraController
  ↓
Camera
```

이를 통해 입력 방식과 카메라 상태를 분리했습니다.

### 3. 공용 Mesh 구조

`Mesh`는 어떤 물체인지 알지 못하고 Vertex / Index Buffer만 관리합니다.

현재 Vertex 구조는 이후 확장을 고려해 다음 정보를 포함합니다.

```text
Position
Normal
UV
Color
```

### 4. 초기화 오류 진단

초기화 실패 시 단순히 프로그램이 종료되지 않고 실패 단계를 구분해 출력합니다.

확인 가능한 주요 단계:

```text
Window
Renderer
Shader
Constant Buffer
Mesh
```

Shader 오류의 경우 HLSL 컴파일 메시지도 함께 출력합니다.

## Camera 조작

| 입력 | 기능 |
|---|---|
| `W / A / S / D` | 현재 카메라 방향 기준 이동 |
| `마우스 좌클릭 + Drag` | Target 중심 Orbit 회전 |
| `마우스 휠` | Zoom In / Out |

Camera가 Object를 회전시키는 방식이 아니라 **Camera 자체가 Target 주변을 회전**합니다.

## 렌더링 흐름

```text
Transform
   ↓
World Matrix

Camera
   ↓
View / Projection

World × View × Projection
   ↓
Constant Buffer
   ↓
Vertex Shader
   ↓
Mesh::Draw()
```

## 테스트 Object

Framework 검증용 Cube를 생성합니다.

Cube는 공용 `Mesh`를 이용해 생성되며, 각 면에 다른 Vertex Color를 사용하여 Camera 회전과 Back-Face Culling 상태를 쉽게 확인할 수 있습니다.

## 빌드

Visual Studio 2022 기준:

```text
1. DXFramework.sln 실행
2. Debug | x64 선택
3. F5
```

사용 라이브러리:

```text
d3d11
dxgi
d3dcompiler
```

## 인코딩

한글 주석이 깨지지 않도록 다음 기준을 사용합니다.

```text
.cpp / .h
→ UTF-8 BOM

.hlsl
→ UTF-8 without BOM

MSVC
→ /utf-8

Character Set
→ Unicode
```

HLSL은 `D3DCompileFromFile()`과의 호환을 위해 BOM 없는 UTF-8을 사용합니다.
