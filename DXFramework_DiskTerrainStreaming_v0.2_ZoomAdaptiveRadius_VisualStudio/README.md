# Large Terrain — Zoom-adaptive Disk Tile Streaming v0.2

## 목적

이 프로젝트는 이전 Terrain Streaming v0.5를 별도로 보존하면서 **새 프로젝트에서는 대형 지형 하나만 실행**한다.
기존 Cube/Perlin/HeightMap/QuadTreeLOD/Triplanar/Chunk/GPU Streaming Showcase는 새 `Application`에서
**초기화하지도 렌더링하지도 않는다.** 공용 카메라/렌더러, QuadTreeLOD, Triplanar Material만 재사용한다.

## 대형 지형과 파일 구성

- 1024 × 1024 Cell = 1025 × 1025 원본 Vertex 상당 (이 전체 Vertex 배열은 런타임에 생성하지 않음)
- CellSize 0.25, 세계 좌표 X/Z = -128 ~ +128.
- 64 × 64 Cell / Tile, 16 × 16 = 256개 Tile.
- `Assets/TerrainTiles/Terrain.meta`만 시작 시 읽음. 각 Tile의 `min/max Height` 및 위치는 Metadata로 판정.
- `Assets/TerrainTiles/Tile_XX_ZZ.tile`은 해당 Tile이 요청되었을 때 **실제 `ifstream`으로 읽음**.
- Tile 내부 (65×65) 고유 Vertex에 좌우 위아래 Halo Sample 1개씩 추가해 67×67 Float 높이 저장.
- 인접 Tile은 동일한 원본 격자 경계 높이와 Halo 중앙차분 Normal을 사용하므로 높이/Normal 일치.
- 생성된 파일을 다시 만들려면 `python Tools/generate_tiles.py` (Python3 + numpy 필요).
  이 Python 스크립트는 **오프라인 생성기이며 게임 실행 중 사용되지 않음**.

## 실제 실행 파이프라인

```text
Terrain.meta (초기 / 소량의 Bounds 및 설정만 읽음)
    -> Camera Target / Tile AABB 사이의 XZ 최단 거리 검사
    -> Zoom Distance에 따라 동적으로 Load 28~160 / Unload 최대 188 (Hysteresis)
    -> 최대 Pending 12개, 가까운 순으로 Worker에 요청
    -> Worker: 해당 .tile 1개 비동기 파일 읽기
    -> Worker: Vertex+Smooth Normal+QuadTreeLOD+표면 Border 생성
    -> 메인 스레드: 프레임당 최대 2개 GPU VB/IB 업로드
    -> Frustum Culling + QuadTree LOD + Triplanar Render
    -> 필요 없어진 Tile의 GPU VB/IB, CPU QuadTree 해제
```

전체 지형의 CPU HeightMap/전체 Vertex 버퍼를 시작 시 한 번에 로드하는 이전 GPU-only Streaming과 다름.
Meta에 저장된 Tile별 Min/Max Height와 Bounds는 필요하며 메모리에 유지된다.
GPU Residency는 Tile GPU Vertex/Index Buffer 수치이며 Driver 전체 VRAM 사용량이 아님.
CPU Tile Samples/CPU MeshData는 Worker와 업로드 중 **일시적으로** 생성되며, 장기 보관하지 않는다.
따라서 `CPU full HeightMap: 0 KiB`는 **전체 CPU 메모리 사용량이 0이라는 의미가 아님**.
실제 디스크 Cache/OS Paging/드라이버 VRAM 총량은 포함하지 않는다.

## 새 기능: 마우스 휠 연동 Streaming 거리 (v0.2)

- 마우스 휠로 Camera Distance를 줄이면 **Load / Unload 반경이 함께 감소**, 멀어지면 증가한다.
- 기존 `CameraController`와 `Camera`, Tile 파일 로딩 Worker, QuadTree LOD, Triplanar Material은 수정하지 않는다. `Application`이 카메라 거리만 `DiskTerrainManager::Update`에 전달한다.
- 거리 계산은 `ZoomStreamingRadiusPolicy.h` 안의 독립적인 순수 함수가 처리한다.
- `Camera Distance=46`에서는 **Load 56 / Unload 84**로 이전 초기값과 동일하다.
- `Camera Distance=2`에서는 **Load 28 / Unload 56**, `Camera Distance=200`에서는 **Load 160 / Unload 188**이며 더 멀리 Zoom Out해도 이 최대값을 초과하지 않는다.
- 계산식: `Load = clamp(56 + (CameraDistance-46)*0.75, 28, 160)` / `Unload = min(Load+28,188)`.
- **최대 Load Radius 160, 최대 Unload Radius 188**은 `Application.cpp`에서 조절할 수 있다. `ZoomRadius.Enabled=false`로 설정하면 v0.1 고정 반경 56/84로 돌아간다.
- Loading 판정은 Camera Target에서 Tile **AABB까지의 XZ 최단 거리**다. Tile 중심이 원 안에 있어야 하는 방식이 아니다.
- Zoom 변화는 즉시 판정에 반영되지만, 비동기 파일 읽기/GPU 업로드 한도로 화면의 Tile은 조금 뒤에 나타날 수 있다.
- 미니맵은 **같은 실제 Load/Unload 반경**을 받아 초록/주황 원을 함께 변경한다. 미니맵의 표시 배율도 범위가 넓어지면 조절한다.
- F3 통계에 현재 `Zoom / Load / Unload / Max Load`를 표시한다. 실측은 `Loaded / Desired / Pending` 및 `Resident GPU`로 확인한다.
- **주의:** 이 기능은 GPU에 상주시키는 Tile의 거리 판정 범위를 조절한다. 카메라 Frustum/FOV 자체를 자르거나 렌더링을 원형으로 클리핑하는 기능은 아니다. 기존 F1 Frustum Culling은 그대로 작동한다.
- F5 Streaming OFF 시에는 기존대로 전체 256 Tile 요청이 우선되므로 Zoom 반경 제한이 적용되지 않는다. 반경 자체는 미니맵에 표시된다.

## 검증 (Zoom 연동)

1. F5 ON / F3 ON / F6 ON에서 카메라 마우스 휠 Zoom In → `Load / Unload` 감소, 초록/주황 원 변화 확인.
2. Zoom Out → `Load / Unload` 증가. 최대로 Zoom Out해도 `Load 160 / Unload 188`을 넘지 않는지 확인.
3. 충분히 기다리면 `Desired`, `Loaded` Tile 수 및 GPU Mesh Buffer 사용량이 변화하는지 확인.
4. WASD로 이동해도 Zoom 거리 기준 반경이 그대로 유지되는지, F1/F2가 정상인지 확인.
5. F5 OFF 시 전체 Tile이 점진적으로 로드되는 기존 비교 동작 유지 확인.

`Tests/test_zoom_streaming_radius_policy.cpp`는 Windows SDK 없이 독립적으로 거리 정책을 검사할 수 있다.

## 조작

| 키 | 동작 |
| --- | --- |
| WASD | Camera Target 이동 (속도 30 world units/s) |
| 마우스 좌클릭 드래그 | Orbit 카메라 회전 |
| 마우스 휠 | Zoom + Streaming Load/Unload 반경 연동 (최대값 제한) |
| F1 | Frustum Culling ON/OFF |
| F2 | QuadTree LOD ON/OFF |
| F3 | 작은 안내/통계 패널 표시 토글 |
| F4 | 지형 표면 Tile 경계선 표시 토글 |
| F5 | Disk Streaming ON/OFF (OFF = 전체 Tile을 요청/유지; 대량 로딩 주의) |
| F6 | 회전하는 지역 Streaming 미니맵 ON/OFF |

- 초기 화면에는 **Large Terrain 1개**만 렌더링되고 F3 패널은 접힌 상태, F6 미니맵은 ON.
- 미니맵은 **카메라 방향이 위쪽**이며 N/E/S/W 나침반 표시를 유지한다.
- 미니맵은 Camera Target 주변의 가변 지역(`max(85, UnloadRadius×1.12)`)을 표시한다.
- 미니맵 초록 원 = 신규 요청 Load Radius, 주황 원 = 기존 로딩 요청·로드 결과 유지 Unload Radius.
- 실제 판정은 **원 안에 Tile 중심이 존재하는지**가 아니라 **원 중심에서 Tile AABB까지의 XZ 최단 거리**.
- Loaded=녹색, Pending=노랑, Desired=파랑, Unloaded=회색, Failed=빨강.

## 검증 순서

1. `Debug | x64`로 Build. 시작 화면에서는 중심 Tile부터 점진적으로 지형이 나타남.
2. F3을 열어 `Disk Reads`가 증가하는지 확인. CPU 전체 HeightMap은 생성하지 않음.
3. WASD로 멀리 이동할 때 Loaded/Desired/Pending 수와 GPU Mesh Bytes 변하는지 확인.
4. F6에서 초록/주황 원과 Tile 상태를 확인. 마우스 회전에도 W는 미니맵 위쪽.
5. F4를 켜서 Tile 접합부에 실제 구멍/Normal 이음매가 없는지 확인.
6. F5 OFF면 지형 전체 256개가 프레임당 최대 2개씩 점진적 로드. 테스트 후 ON으로 복귀.
7. 실행 중 Tile 파일 하나를 지우고 해당 위치 이동 -> F3 Failed/Error 확인 (원상 복구 후 재시작).

## 설계/제한

- Win32/D3D11 렌더 스레드만 GPU 리소스를 생성/해제, Worker는 File I/O+CPU만 담당.
- 이미 로드/요청된 Tile은 현재 Zoom에서 계산된 Unload Radius까지 유지하여 거리 경계 깜빡임 억제.
- Worker 취소 및 결과 처리에 Generation 비교, 최대 Pending 12개로 오래된 작업 대기 제한.
- 원본 파일은 **미리 분리한 높이 Tile**이므로 운영 중 HeightMap PNG 전체 디코딩을 하지 않는다.
- Texture는 Grass/Rock/Snow 3개를 공용 공유 (Texture Virtual Streaming은 이번 범위 밖).
- 인접 Tile 사이의 서로 다른 LOD로 인한 간극은 기존 QuadTreeLOD Skirt로 완화.
- Disk Streaming을 OFF하면 전체를 메모리에 올리므로 최적화 성능 비교용으로만 권장.
- 현재 타일 동기화/파일 포맷은 고정 크기 float32, little-endian 환경(Windows x64)을 전제.
- Tile 로드 우선순위는 거리 기반이며 카메라 빠른 이동 시 순간적인 빈 영역은 발생 가능 (프리페치/시각적 Fade는 다음 단계).
- 미니맵/기존 상세 Overlay는 각각 독립적으로 켜고 끌 수 있다.
- 이전 단계의 전체 프로젝트 ZIP/Repo는 별도로 보존한다. 현재 프로젝트에서 Legacy Showcase 객체는 생성하지 않는다.

## 디렉터리

```text
Source/Features/DiskTerrainStreaming/
    TileArchive.h/.cpp          : 파일 포맷/Meta/Tile 읽기 (DirectX 미의존)
    TileGeometryBuilder.h/.cpp  : Halo Normal / Tile Mesh / QuadTree / Border
    DiskTileWorker.h/.cpp       : Thread, Queue, Cancellation
    DiskTerrainManager.h/.cpp   : Radius Policy, Main-thread GPU/Render/Stats
    StreamingRangeDebugRenderer.h/.cpp : 회전형 지역 미니맵
Source/Features/QuadTreeLOD/   : 이전 계층형 LOD 알고리즘 재사용
Source/Features/TriplanarTerrain/TriplanarMaterial.* : 이전 Material 재사용
Assets/TerrainTiles/          : 실제 독립 Tile 256개 + Terrain.meta
Tools/generate_tiles.py       : 오프라인 Tile 데이터 생성기
```

## 코드 인코딩

`.cpp/.h`: UTF-8 BOM 및 `/utf-8` / `.hlsl`: UTF-8 without BOM.

## 빌드 검증

Linux 환경에서는 Windows SDK / MSVC / Direct3D11 실제 앱 실행이 불가하므로 **Windows Debug x64 빌드 및 렌더링 테스트는 사용자가 직접 확인해야 함**.
패키지 생성 시 Meta/Tile 로더 C++ 단독 테스트와 파일 접합부 높이·Normal 검사를 수행한다.
