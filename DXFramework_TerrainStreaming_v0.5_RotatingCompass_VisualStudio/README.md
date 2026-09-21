# Terrain Streaming v0.5 — 카메라 방향 연동 미니맵 + 나침반

기존 v0.4의 화면 우하단 2D 미니맵에 **카메라 방향 연동 회전**과 **N/E/S/W 나침반**을 추가한 버전입니다.

## 변경 이유

기존 미니맵은 `World +X → 화면 오른쪽`, `World +Z → 화면 아래쪽`으로 고정돼 있었습니다. 반면 WASD는 카메라 기준이므로, 카메라를 회전하면 W를 누를 때 위로 움직여도 A/D가 지도 화면의 좌우와 반대로 보일 수 있었습니다.

이번 버전은 `View Matrix`에서 카메라의 수평 Forward/Right를 구해 **전방을 항상 미니맵 위쪽, 카메라 기준 오른쪽을 미니맵 오른쪽**으로 맞춥니다. 카메라를 마우스로 회전하면 지형 격자와 Chunk 상태가 그 방향에 맞춰 함께 회전합니다. WASD나 카메라 이동 로직은 수정하지 않았습니다.

## 나침반 방향

미니맵 우상단에 문자형 `N / E / S / W` 나침반을 추가했습니다. 이 프로젝트에서 북쪽은 **World +Z**, 동쪽은 **World +X**로 정합니다. 문자들의 위치는 카메라 방향에 따라 회전하지만, 글자 자체는 똑바로 유지합니다. 가운데 화살표는 현재 카메라가 바라보는 수평 전방을 나타내며 항상 위를 가리킵니다.

> 이 프로젝트에는 별도 Player Actor의 회전이 아니라 Orbit Camera의 회전이 있으므로, 미니맵은 Player가 아닌 **카메라 Yaw**를 따라갑니다. Pitch를 바꿔도 지도가 뒤집히지 않습니다.

## 변하지 않은 부분

- Streaming Policy: LoadRadius 1.9 / UnloadRadius 2.9, Chunk AABB까지 XZ 최단 거리
- Chunk CPU Worker / GPU Mesh Load·Unload / QuadTree LOD / Frustum Culling / Triplanar
- Input/Camera/CameraController의 이동·회전 방식
- `7` Streaming Terrain, `F3` 설명 패널, `F4` Chunk 경계, `F5` Streaming, `F6` 미니맵 표시
- 초록 원은 새 Load 요청 범위, 주황 원은 기존 Loaded/Pending 유지 범위
- Chunk 상태 색상 및 미니맵 크기와 우하단 위치

## 구현 범위 / 제거 방법

수정한 코드는 `Source/Features/TerrainStreaming/StreamingRangeDebugRenderer.cpp/.h`뿐입니다. 화면 회전과 나침반은 이 디버그 Renderer 내부에서 처리합니다. 기능을 제거할 때는 이 두 파일만 v0.4로 되돌리면 됩니다. 기존 프로젝트별 README는 `README_v0.4_previous.md`로 보관했습니다.

## 검증 방법

1. `7 ON`, `F6 ON`, `F3 OFF`로 우하단 미니맵을 확인합니다.
2. 마우스로 수평 회전할 때 미니맵의 Chunk 격자 및 N/E/S/W 글자 위치가 함께 회전하는지 확인합니다.
3. 어느 각도에서든 W=지도 위, S=아래, A=왼쪽, D=오른쪽으로 이동하는지 확인합니다. (미니맵은 필요에 따라 전체 지형과 원을 포함하도록 화면에 맞게 자동 스케일/평행이동하므로 십자의 화면 위치가 움직일 수도 있습니다.)
4. F5 ON에서 Focus 이동에 따라 Loaded/Pending/Desired 색상이 기존대로 변하는지 확인합니다.
5. F6 OFF에서 미니맵만 사라지고 Streaming 동작은 그대로인지 확인합니다.

**참고:** 본 패키지는 코드/구성 정적 검증 대상이며 이 환경에서는 MSVC/Windows DirectX 실행을 검증할 수 없습니다.
