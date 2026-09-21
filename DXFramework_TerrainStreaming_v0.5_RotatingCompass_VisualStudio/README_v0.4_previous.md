# Terrain Streaming v0.4 — Screen-space Range Mini Map

이번 단계는 **이전 v0.3의 대형 3D 원기둥 디버그 표현만 교체**합니다. Terrain Streaming 판정, 비동기 Worker, GPU Chunk 관리, 기존 QuadTree LOD / Culling / Triplanar 코드에는 변경이 없습니다.

## 바뀐 이유

3D 원기둥은 화면과 지형을 가리고 수직 가이드가 너무 많아 거리 판정을 읽기 어려웠습니다. v0.4부터는 3D 원기둥을 그리지 않고, 화면 **우하단 최대 약 268×268px의 Top-Down Mini Map** 하나만 표시합니다. 카메라를 수평/측면으로 돌려도 2D 지도는 동일하게 보입니다.

## 조작

- `7`: GPU Streaming Terrain 표시 ON/OFF
- `F3`: 기존 전체 텍스트 디버그 정보 ON/OFF
- `F4`: 월드 공간 Chunk 경계선 ON/OFF
- `F5`: 실제 Streaming ON/OFF (OFF 시 거리 무관하게 모두 점진적 로딩)
- `F6`: Streaming Mini Map ON/OFF (기본 ON)
- `WASD`: Camera Target 이동 = Streaming Focus 이동

## Mini Map 읽는 방법

- **녹색 원:** 새 Chunk 요청 범위 `LoadRadius = 1.9`
- **주황 원:** 이미 로드되었거나 Pending인 Chunk를 유지하는 범위 `UnloadRadius = 2.9`
- **흰 십자:** Camera Target(Streaming Focus). Camera 실제 Position이 아님.
- **격자 청록색:** GPU Mesh 로딩 완료(Loaded)
- **격자 노란색:** 비동기 로딩 요청 중(Pending)
- **격자 회색:** GPU Mesh가 없는 Chunk(Unloaded)
- **격자 파란색:** 필요한 것으로 판정됐으나 아직 로딩 요청/완료 상태가 아닌 Chunk(Desired)
- **격자 붉은색:** Chunk 로딩 실패(Failed)

`F3`의 텍스트 통계에는 색상 의미와 Loaded/Desired/Pending 숫자가 표시됩니다. Mini Map 하단의 5개 짧은 막대는 순서대로 **녹색 Load / 주황 Keep / 청록 Loaded / 노랑 Pending / 회색 Unloaded** 색상 키입니다.

**중요:** 두 원은 World XZ 평면의 거리 가이드입니다. 실제 판정은 Camera Target과 **Chunk Bounding Box 사이의 XZ 최단 거리**를 사용하므로, Chunk 중심이 원 밖에 있어도 가장자리가 원과 교차하면 대상이 될 수 있습니다. 주황 원은 새로 로딩하는 범위가 아니라 이미 로드/요청된 Chunk의 유지 범위입니다. `F5 OFF`면 두 원은 참고 표시만 됩니다.

## 확장성과 제거

- 기존 Feature 파일 `StreamingRangeDebugRenderer.h/.cpp`의 구현만 3D 원기둥에서 2D Mini Map으로 변경했습니다.
- `TerrainStreamingManager.cpp`에서 실제 Chunk 상태(`Loaded/Pending/Desired/Failed` 및 World Bounds)를 복사해 Mini Map에 전달하는 부분만 추가했습니다.
- `Application.cpp`의 F3 설명 문구만 수정했습니다. F6과 7번 기존 입력 체계 유지.
- 일반 Framework/Renderer/Camera/Shader/Mesh/HeightMap/QuadTreeLOD/Triplanar/StreamingWorker/StreamingPolicy 변경 없음.
- 범위 표시를 원하지 않으면 F6으로 끄거나 `StreamingRangeDebugRenderer` 연결부와 해당 파일 둘을 제거할 수 있습니다.
- 기존 v0.3 README는 `README_v0.3_previous.md`에 보관합니다.

## 테스트

1. Visual Studio `Debug | x64` 빌드. 이 환경에서는 MSVC/DirectX 실행 검증을 할 수 없으므로 실제 Windows 빌드 확인이 필요합니다.
2. 7 ON, F6 ON: 월드 위 대형 원기둥 없이 우하단 Mini Map만 표시되는지 확인합니다.
3. WASD 이동: 흰 십자가 움직이고 Chunk 색상/로드 숫자가 변하는지 확인합니다.
4. 측면 Orbit: 지도 크기/가독성이 그대로 유지되는지 확인합니다.
5. F6 OFF: 지도만 숨겨지고 실제 Streaming과 F4 경계선은 그대로 작동해야 합니다.

`.cpp/.h` 인코딩은 UTF-8 BOM, 기존 `.hlsl`은 BOM 없는 UTF-8을 유지합니다.
