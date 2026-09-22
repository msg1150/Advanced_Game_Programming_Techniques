# Large Terrain — Disk Streaming v0.6: Predictive Prefetch + LRU Tile Cache

## 이번 단계

기준은 기존 `DXFramework_DiskTerrainStreaming_v0.5_NumericInput_VisualStudio`이다. 원래 v0.5 프로젝트와 저장소 루트 README는 변경하지 않았다. DirectX 11/Win32 + Direct2D/DirectWrite **직접 구현한 UI Framework**를 그대로 사용한다. Dear ImGui는 사용하지 않는다.

**이 단계에서 Prefetch와 캐싱을 함께 구현한다.** 현재 요구된 Tile의 디스크 읽기/CPU LOD 생성/GPU 업로드를 최우선으로 처리하며, 카메라가 이동하는 전방에 있는 아직 불필요한 Tile의 **높이 Sample만** 미리 읽어서 CPU LRU Cache에 저장한다. 실제 로딩 대상으로 바뀌면 캐시에서 재사용하므로 같은 `.tile` 파일을 다시 읽지 않을 수 있다. Prefetch 자체로 GPU Terrain Mesh를 미리 업로드하거나 화면에 지형을 표시하지는 않는다.

## 조작 / UI

프로젝트를 Visual Studio 2022에서 `DXFramework.sln` → `Debug | x64`로 빌드/실행한다. 기본 크기 1280×720에서는 설정 UI가 상단 **2열**로 표시되어 미니맵(우하단)과 겹치지 않게 배치된다. 좌측 기존 체크박스 6개와 `Load Radius`, `MiniMap Zoom (%)` 및 숫자 직접 입력을 유지한다. 우측 신규 항목:

| 설정 | 기본값 | 의미 |
| --- | --- | --- |
| `Predictive Prefetch` | ON | Camera Target 이동 방향으로 필요한 파일을 낮은 우선순위로 미리 읽음 |
| `CPU Tile Sample Cache` | ON | 읽은 1-Pixel Halo Height Sample을 LRU 메모리에 보관 |
| `Prefetch Lead` | 48 World Units | 미래 위치 예측 거리의 **상한**. 실제 선행 거리는 `min(이동속도 × 0.9초, 설정값)` |
| `Cache Budget (MiB)` | 1 MiB | 높이 Sample 캐시 용량 상한. UI로 1~16 MiB 조정 가능 |

각 슬라이더의 우측 숫자를 클릭하여 직접 입력 → `Enter` 적용, `Esc` 취소. WASD 이동 및 마우스 회전은 기존대로. 마우스 휠은 **카메라 확대/축소만** 조절하고 Load Radius나 Prefetch Lead에 관여하지 않는다. 숫자 입력 중 WASD 카메라 이동은 차단한다. F1~F6 단축키에 토글을 다시 추가하지 않았다. 기존 기능 ON/OFF는 모두 왼쪽 UI 체크박스에서 수행한다.

`CPU Tile Sample Cache`를 OFF하면 보관된 Samples를 즉시 비우고 Prefetch를 중단한다. `Predictive Prefetch`만 OFF하면 speculative 읽기는 중단하되 이미 읽힌 캐시와 일반 Tile 로딩의 재사용은 유지한다. `Disk Streaming`을 OFF하면 전체 256개 Tile이 기존처럼 점진적으로 GPU에 로드되고 Prefetch는 수행하지 않는다(성능 비교용 설정).

## 내부 처리 순서

```text
매 프레임 Camera Target 위치와 deltaTime 수집
  ├─ 기존 Load / Unload 반경: GPU에 필요한 실제 Tile 판정 (그대로)
  └─ 이전 Target과의 XZ 변화 → 속도 평활화 → 미래 Focus 계산
         ↓
     예측 지점의 Load Radius 내 Tile 중 현재 필요하지 않은 Tile 선택
         ↓  최대 12개, 예측 지점과 가까운 순
     Worker의 Prefetch 큐 교체 (이전 방향 대기 작업 취소)
         ↓ 필수 Tile Request를 항상 우선 처리
     Worker: Prefetch 대상 파일의 CPU 높이 Sample만 읽어 LRU에 저장
         ↓
     실제 Tile 요청 시 Cache Hit → 디스크 재읽기 생략
         ↓
     기존 CPU Geometry / QuadTree LOD Build → Main Thread GPU Upload
```

`MaxPending=12`, `MaxGpuUploadsPerFrame=2` 기존 상한 유지. 필수 요청을 큐에서 먼저 꺼내기 때문에 speculative 작업으로 필수 작업이 밀리지 않는다. 이미 실행 중인 단일 파일 읽기 자체는 선점되지 않아 즉각적인 취소를 보장하지 않는다. 예측 목록이 바뀌면 이전 큐는 즉시 교체하고, 더 이상 필요하지 않은 완료 결과는 캐시에 넣지 않는다. Worker가 파일 읽기·CPU 연산을 담당하고 Direct3D Device/Context 사용은 Main Thread에만 남는다.

캐시는 **전체 원본 HeightMap이 아니라 개별 `.tile`의 높이 Sample만** 저장하고, 최근 사용한 Tile을 유지하는 LRU 방식으로 용량을 넘기면 오래된 데이터를 제거한다. Cache Hit에서도 QuadTree/Mesh CPU 재생성은 수행하고, GPU Mesh는 이전처럼 Unload Radius 밖에서 해제된다. 캐시의 1 MiB 한도는 높이 Sample 벡터 데이터 기준이며 컨테이너·일시 버퍼·GPU/드라이버 메모리를 합산한 프로세스 메모리 제한이 아니다.

## Debug Statistics에서 확인할 항목

- `Disk Reads / KiB`: **필수 + Prefetch를 합친** 실제 성공한 파일 읽기 횟수와 파일 바이트 수. 누적값.
- `Prefetch disk reads`: 그중 예측 로딩이 성공한 횟수. 누적값.
- `Cache tiles / KiB`: 현재 캐시에 보유한 Tile Sample 수/바이트 수.
- `Cache hits / evicted`: 일반 로딩 요청이 캐시를 재사용한 누적 횟수 / 용량 초과로 제거된 횟수.
- `Prefetch queue / fail`: 현재 대기 중인 speculative 요청 개수 / speculative 파일 읽기 실패 누적 횟수.
- 기존 Loaded/Desired/Pending, GPU Buffer KiB 등은 그대로 출력한다. 캐시는 GPU에 실제 상주하는 Tile 수와 별도로 계산된다.

누적 Disk Reads는 Prefetch를 켠 직후 **오히려 증가할 수 있다.** Prefetch의 목적은 새 위치에 도달하기 전에 읽기를 앞당기는 것이고, Cache Hit가 발생하면 그 Tile의 추가 Disk Read가 발생하지 않는지를 확인해야 한다. 동일한 길을 되돌아올 때 Cache Budget 범위 내 데이터라면 Hit가 늘어난다.

## 빠른 테스트

1. 실행 후 왼쪽 `Debug Statistics` ON, 오른쪽 Prefetch/Cache ON 확인. WASD로 직진하면서 `Prefetch disk reads` 및 `Cache tiles` 증가 확인.
2. 이동하던 방향으로 계속 진행할 때 `Cache hits`가 늘어나는지 확인한다(속도와 로딩 타이밍에 따라 다를 수 있음).
3. 방금 왔던 길로 되돌아가서 `Cache hits`가 늘어나는지 확인. 이미 GPU에 상주한 Tile이면 CPU 재로딩이 불필요해 Hit가 발생하지 않을 수 있으므로 충분히 멀리 이동한 뒤 복귀한다.
4. `Predictive Prefetch` OFF → Cache는 계속 사용하되 이후 새 speculative read가 생기지 않는지 확인. 이미 시작된 한 건의 I/O는 완료될 수 있음.
5. `CPU Tile Sample Cache` OFF → Cached Tiles/Bytes가 0으로 바뀌고 이후 Hit가 늘지 않는지 확인.
6. `Cache Budget`를 1 → 4 → 1 MiB로 조절하여 메모리 상한과 Eviction 확인(사용량이 새 제한을 초과한 경우에만 발생).
7. `Prefetch Lead`를 0으로 설정하면 예측 요청이 사라지는지, 48로 복원하면 다시 생기는지 확인. Load Radius와 Camera Zoom은 독립 유지.
8. 빠른 방향 전환/Alt+Tab/창 크기 변경/UI 숫자 입력을 반복하여 카메라 조작과 디스크 로딩이 안정적인지 확인.

## 변경 파일

- `Source/Features/DiskTerrainStreaming/PrefetchPolicy.h` : 프레임워크/OS 독립 예측 오프셋 계산.
- `Source/Features/DiskTerrainStreaming/TileSampleCache.h` : 높이 샘플 전용 LRU, 메모리 예산 적용.
- `DiskTileWorker.h/.cpp` : 필수 우선 Prefetch 큐, CPU Cache, 실측 Metrics, 취소·실패 반복 방어.
- `DiskTerrainManager.h/.cpp` : 카메라 이동 예측, 후보 선정, Worker 설정, UI Getter/Setter 및 통계 연결.
- `UIManager.h/.cpp` : 기존 UI를 2열 배치. 공통 Widget 코드 변경 없음.
- `TerrainSettingsPanel.cpp` : 신규 체크박스/슬라이더 및 누적 통계 표시.
- `Application.cpp` : Terrain Update에 기존 deltaTime 전달(카메라 입력 유지).
- `DXFramework.vcxproj`, `.filters`, `CMakeLists.txt` : 신규 Header 등록.
- `Tests/test_prefetch_cache.cpp` : Windows SDK 비의존 예측·LRU 정책 단독 테스트.

기존 256개 Tile 파일, Tile 포맷, TileArchive, TileGeometryBuilder, Shader, QuadTree LOD, Triplanar, Camera/CameraController 및 미니맵 렌더러 코드는 변경하지 않았다. 이전 v0.5 README의 상세 내용은 `README_previous_v0.5.md`에 별도로 보존한다. **저장소 루트 README는 이 프로젝트 생성 과정에서 수정하지 않았다.**

## 검증 범위

`clang++ -std=c++20 -I Source Tests/test_prefetch_cache.cpp -o test_prefetch_cache`로 예측 및 캐시 계산을 독립 실행할 수 있다. 이 작업 환경에서는 플랫폼 독립 테스트와 디스크 Worker의 별도 테스트용 CPU Geometry 스텁을 이용한 I/O/Cache 재사용 테스트를 수행했다. 다만 **실제 Windows MSVC Debug x64 빌드와 DirectX 11 GPU 렌더링, UI 클릭·실행 검증은 하지 못했으므로** 사용자 PC에서 확인해야 한다.
