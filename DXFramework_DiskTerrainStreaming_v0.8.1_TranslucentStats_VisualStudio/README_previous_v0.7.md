# Large Terrain — Disk Streaming v0.7: Cache Miss + Disk/Tile Load Timing

## 변경 목적

정상 실행을 확인한 v0.6 Prefetch + LRU Cache를 그대로 보존하고, **성능 검증용 계측 세 가지**를 추가한다. 이 프로젝트의 다른 버전 및 GitHub 루트 README는 수정하지 않는다. Prefetch 정책, 캐시 용량/LRU, Tile 파일, 카메라·휠·UI 슬라이더 동작은 바꾸지 않았다.

## 통계의 정확한 뜻

| 통계 | 의미 / 집계 기준 |
| --- | --- |
| `Cache hits / misses` | **필수 GPU Tile 요청**에서 CPU Tile Sample Cache를 조회해 성공/실패한 횟수. 누적. Prefetch의 Contains 검사 제외. Cache OFF에서는 조회 자체를 생략하므로 둘 다 증가하지 않음. |
| `Disk read ms L/A` | Worker가 `TileArchive::ReadTile`을 호출하기 직전부터 반환 직후까지 측정한 **마지막 / 평균 ms**. 필수 로드와 Prefetch 모두 포함. 성공한 파일 읽기만 기록하므로 샘플 수 = `Disk Reads`. OS 캐시가 처리한 읽기도 포함됨. |
| `Disk read total ms` | 성공한 `ReadTile` 호출 시간 합계. 앱 전체에서 파일 I/O만 차지한 시간이라는 의미는 아님. |
| `Tile load ms L/A` | 필수 Tile 요청을 Main Thread에서 큐에 등록한 시각부터 **GPU Vertex/Index Buffer 생성 함수가 성공해 Install을 마친 시각**까지의 마지막 / 평균 ms. Worker 대기, 캐시/파일 읽기, CPU LOD·Mesh 생성, 메인 스레드 결과 대기·GPU 리소스 생성 호출을 포함. |
| `Tile load total ms` | 성공적으로 설치한 각 Tile의 경과시간 합계. 여러 요청의 큐 대기가 겹치므로 전체 애플리케이션 실행시간이나 GPU 실행시간은 아님. |
| `Tile loads completed` | Tile Load Time 평균의 분모. 정상적으로 GPU 리소스 설치까지 끝난 **필수 요청** 수. 취소·오류 결과 및 Prefetch만 완료된 결과 제외. |

시간 측정에는 `std::chrono::steady_clock`을 사용하고 내부에서 마이크로초로 누적한 후 UI에는 소수점 둘째 자리 ms로 보여준다. 샘플이 없을 때 시간은 `-`로 표시한다. `Disk Read Time`은 실제 저장장치 하드웨어 시간만이 아니라 C++ 파일 열기, 읽기, 검사 및 OS 캐시의 영향까지 포함한다. `Tile Load Time`은 GPU 명령 제출/버퍼 **생성 호출** 완료까지이지, GPU에서 최종 Draw가 끝나는 시간은 아니다. Prefetch 작업은 Tile Load Time에 직접 기록하지 않지만, 필수 로드가 캐시를 재사용하거나 Worker를 기다리는 데 영향을 줄 수 있다.

## 사용 / 테스트

1. Visual Studio 2022 `DXFramework.sln` → `Debug | x64` 빌드·실행. 좌측 `Debug Statistics` 체크박스 ON.
2. 시작 직후 `Disk Reads`, `Disk read ms L/A`, `Tile loads completed`, `Tile load ms L/A`가 증가하는지 확인.
3. 우측 Cache ON을 유지하고 같은 경로를 GPU Unload Radius 바깥까지 왕복. 재방문하면서 `Cache hits` 증가, 재사용한 Tile만큼 새 `Disk Reads`가 발생하지 않는지 비교.
4. Cache OFF 후 **다른 위치**로 이동하여 hits/misses 값은 더 늘지 않고 Disk Reads 및 Read Time이 증가하는지 확인. 이 때 Prefetch는 같이 중단됨.
5. Cache ON에서 캐시되지 않은 위치로 이동하면 Cache Miss가 증가할 수 있다. Prefetch가 먼저 읽어둔 경우에는 Cache Hit이므로 Miss가 반드시 증가하는 것은 아님.
6. **정확한 비교는 프로그램을 재시작해 누적 통계를 초기화하고** 같은 이동 경로·같은 Zoom·같은 Radius에서 Prefetch/Cache 설정만 바꿔 진행. GPU 메모리 절감 및 프레임 타임 개선은 별도 계측이 필요하다.

`Debug Statistics` 패널 높이를 늘려 새 항목이 잘리지 않도록 했으며 우측 설정 패널/미니맵은 그대로다. 작은 창에서 UI가 화면 밖으로 잘릴 수 있으므로 기본 1280×720을 권장한다.

## 수정 파일

- `Source/Features/DiskTerrainStreaming/StreamingTiming.h` : 플랫폼 독립 시간 누적, 마지막/평균/합계 계산.
- `DiskTileWorker.h/.cpp` : 필수 요청 Cache Miss 및 성공한 `ReadTile` 경과시간 계측.
- `DiskTerrainManager.h/.cpp` : 필수 요청 시각 기록, 성공적으로 GPU Tile을 설치한 시각까지의 Tile Load Time 계측, 통계 Snapshot 전달.
- `TerrainSettingsPanel.cpp` : Debug Statistics에 Miss와 시간 표시, 패널 확장.
- `DXFramework.vcxproj`, `.filters`, `CMakeLists.txt` : 새 헤더 등록.
- `Tests/test_streaming_timing.cpp` : `chrono` 시간 변환/누적 테스트.
- `README_previous_v0.6.md` : 이전 단계 설명 그대로 보존.

**기존 TileArchive, Worker 큐 우선순위, TileSampleCache LRU 정책, PrefetchPolicy, Geometry Builder, Shader, Camera, MiniMap/UI 위젯과 256개 Tile 파일은 변경하지 않았다.** Windows MSVC/DirectX 실행은 이 환경에서 검증할 수 없으므로 실제 Debug x64 빌드 및 화면 결과는 사용자 PC에서 확인해야 한다.
