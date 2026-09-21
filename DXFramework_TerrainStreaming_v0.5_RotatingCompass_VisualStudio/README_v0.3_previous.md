# Terrain Streaming v0.1 — GPU Chunk Streaming

이전 **Terrain Chunk v0.3**을 베이스로, 기존 Source/Features/TerrainChunk, QuadTreeLOD,
HeightMapTerrain, TriplanarTerrain, Renderer를 수정하지 않고 **독립적인 선택 Feature**를 추가했다.

## 이번 단계에서 실제로 구현한 범위

- HeightMap 이미지는 실행 시작 시 1회 읽고 전체 Vertex/Normal을 CPU 원본에 보존한다.
- 초기에는 Chunk별 GPU Vertex/Index Buffer를 **전부 만들지 않는다**.
- 카메라가 바라보는 중심(Camera Target) 기준으로 `LoadRadius` 이내 Chunk에 요청을 발행한다.
- 별도 CPU Worker 1개가 요청받은 Chunk의 Vertex, 기존 QuadTreeLOD, 표면 경계선 데이터를 비동기 준비한다.
- 메인 Render Thread만 `ID3D11Device::CreateBuffer`를 호출하며 프레임당 업로드 수를 제한한다.
- `UnloadRadius` 밖 Chunk는 `Resident` 객체를 파괴해 GPU Mesh/Border 버퍼 참조를 해제한다.
- Load / Unload Radius를 다르게 설정해 경계 근처에서 계속 생성·파괴되는 현상을 억제한다.
- 카메라 이동 중 필요 없어진 요청 결과는 Generation 번호로 폐기한다.
- 이미 완성된 Chunk에는 기존 Frustum Culling / QuadTree LOD / Triplanar Material을 재사용한다.
- F4 표면 경계선 및 F3 간단/전체 통계 패널을 유지한다.

**범위 제한:** 높이맵 파일의 디스크 단위 타일 스트리밍이나 텍스처 타일 스트리밍은 구현하지 않았다.
원본 HeightMap Vertex는 CPU에 계속 상주하고, Chunk별 CPU 임시 산출물과 GPU Mesh만 수명 관리한다.
`ResidentMeshBytes`는 신규 7번 Streaming 기능에서 요청한 Mesh VB/IB 바이트만 합산하며,
드라이버의 총 VRAM 사용량이 아니다. **이 Showcase는 기존 1~6번 기능도 시작할 때
초기화하므로, 6번을 화면에서 숨겨도 기존 Chunk GPU 버퍼는 상주한다.**
따라서 프로세스 전체 VRAM 사용량 감소를 증명하는 벤치마크가 아니라 7번
Feature 내부의 리소스 생명주기 테스트다.

## 기존 Feature 보호 / 제거

```text
Source/Features/TerrainChunk          ← 기존 v0.3 변경 없음
Source/Features/QuadTreeLOD           ← 기존 변경 없음
Source/Features/TriplanarTerrain      ← 기존 변경 없음
Source/Graphics                       ← 기존 변경 없음

Source/Features/TerrainStreaming/      ← 신규
  TerrainStreamingWorker.h/.cpp        CPU 비동기 작업
  TerrainStreamingManager.h/.cpp       Main Thread GPU Load/Unload/Render
```

새 기능을 제거할 때 신규 폴더, Application의 신규 include/member/Initialize/Update/Render 연결,
vcxproj/filters/CMake의 신규 항목만 제거하면 기존 6번 Chunk Showcase는 그대로 남는다.

## 조작

| 키 | 기능 |
|---|---|
| 1~5 | 기존 Showcase |
| 6 | 이전 Chunk v0.3 (전부 생성) |
| 7 | 신규 GPU Streaming Showcase |
| 8~0 | 예약 |
| F1 | Culling 토글 (기존/Streaming 동기화) |
| F2 | LOD 토글 (기존/Streaming 동기화) |
| F3 | Debug Panel 토글 (기본 OFF) |
| F4 | 지형 표면 Chunk 경계선 표시 토글 |
| F5 | Streaming ON/OFF; OFF에서는 모든 Chunk를 GPU에 점진적으로 로드하고 유지 |

**기본값:** 1~6 OFF, 7 ON, F1 ON, F2 ON, F4 ON, F5 ON.

## 설정 위치

`Source/Core/Application.cpp`의 `TerrainStreamingSettings`:

```cpp
streamingSettings.CellsPerChunk = 32u;
streamingSettings.LoadRadius = 1.9f;
streamingSettings.UnloadRadius = 2.9f;
streamingSettings.MaxUploadsPerFrame = 2u;
streamingSettings.EnableStreaming = true;
```

현재 테스트 HeightMap 129×129 Vertex이므로 전체 16개 Chunk다. 중앙에선 로드 대상이 일부이고,
WASD로 Camera Target을 옮기면 Loaded/Desired/Pending/ResidentMeshBytes가 바뀐다.
마우스 휠 Orbit Distance는 Camera Target 자체를 이동하지 않으므로 이 버전의 Streaming 중심은 변하지 않는다.

## 권장 테스트

1. 시작 후 7 ON / 6 OFF, F3 ON. `Loaded Chunks`가 비동기적으로 올라가는지 확인한다.
2. WASD로 Terrain의 왼쪽/오른쪽을 오가며 `Desired`, `Loaded`, `ResidentMeshBytes`가 변하는지 본다.
3. F5 OFF로 변경하면 `Loaded Chunks`가 최종적으로 `16 / 16`에 도달한다(Worker 처리 시간 필요).
4. F5 ON으로 되돌리고 멀리 이동하면 Loaded Chunk 수와 Buffer Byte가 줄어드는지 본다.
5. F1/F2/F4를 각각 전환해 기존 Culling/LOD/표면 경계선이 그대로 동작하는지 확인한다.
6. Chunk 생성 중에는 해당 영역에 일시적인 공백이 있을 수 있다. LOD Crack과 별개다.

## 주의

- 작은 HeightMap의 경우 GPU Buffer 절약이 성능 향상으로 바로 연결된다는 뜻은 아니다.
- Streaming ON은 카메라의 현재 화면 직사각형을 사용하지 않고, Camera Target 중심 거리 범위를 쓴다.
- 이 단계는 직렬 Worker 1개를 사용한다. GPU 업로드와 디바이스 접근은 메인 스레드 전용이다.
- 다음 학습 단계 후보: 파일 Tile 인덱스/부분 로딩, 비동기 I/O, 이동 예측 Prefetch, Job 우선순위 및 캐시.

## 인코딩

- .h/.cpp: UTF-8 BOM + /utf-8
- .hlsl: UTF-8 no BOM (기존 Shader 전혀 변경하지 않음)

## 디버그 범위 표시

- `F6` : Streaming Load/Unload 반경선 표시 ON/OFF
- 녹색 원형 선 = Load Radius
- 주황색 원형 선 = Unload Radius
- 원형 중심 = 현재 Camera Target(Streaming Focus)


## v0.3 — 측면에서도 보이는 Streaming 판정 범위

기존 v0.2.1은 Load/Unload 반경을 `Y=0.05`의 평면 원 두 개로 그렸기 때문에,
카메라를 옆으로 돌리면 원이 얇은 선으로 보이거나 산에 가려졌습니다.
이번 버전은 **실제 Streaming Policy는 바꾸지 않고 시각화만 개선**했습니다.

- **초록색 3D 원기둥 테두리:** Load Radius `1.9`.
- **주황색 3D 원기둥 테두리:** Unload Radius `2.9`.
- 각 원기둥은 지형 높이를 감싸는 상단/하단 링과 12개의 수직 가이드로 구성됩니다.
- Debug 전용 패스에서만 Depth Test를 끄므로, 측면과 지형 뒤에서도 범위가 보입니다.
- 기존 Terrain / Chunk Border는 원래 Depth Test를 그대로 사용하며 Debug Pass 후 상태를 복구합니다.
- `F6`으로 이 3D 범위만 ON/OFF할 수 있습니다. `F4`의 Chunk Border는 독립적입니다.
- `StreamingRangeDebugRenderer`에서 Mesh를 초기화 시 한 번 생성하고 카메라 Target 이동 시 Transform만 바꿉니다.

**중요한 판정 해석:** 원기둥은 Focus 중심의 XZ 반경을 3D로 시각화한 참고선입니다.
실제 코드는 Focus와 *청크 AABB 사이의 XZ 최단 거리*를 사용하므로, 청크 중심이 원 밖이어도
청크 가장자리가 원에 닿으면 Load/Keep 대상이 될 수 있습니다. Y 높이는 판정에 사용하지 않으며,
원기둥 위/아래가 별도의 로딩 높이 제한을 의미하지 않습니다.

### 이번 버전의 수정 범위

- 신규 `Source/Features/TerrainStreaming/StreamingRangeDebugRenderer.h/.cpp`
- `TerrainStreamingManager.h/.cpp`: 기존 동적 원 Mesh 생성을 제거하고 새 디버그 Renderer 연결
- `Application.cpp`: F3 범위 설명 문구만 변경, F6 입력 방식 유지
- `DXFramework.vcxproj`, `.filters`, `CMakeLists.txt`: 신규 파일 등록

QuadTreeLOD / HeightMap / Triplanar / StreamingWorker / StreamingPolicy / Graphics 공통 파일은 수정하지 않았습니다.

**검증:** Debug x64로 빌드 → `[7]` ON → `[F6]` ON → 좌클릭 드래그로 수평 시점 확인 →
WASD로 Target 이동 시 두 원기둥이 따라 움직이는지 확인 → F6 OFF에서 두 원기둥만 사라지는지 확인.
