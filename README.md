# 고급게임프로그래밍기법

고급게임프로그래밍기법 수업에서 진행한 실습 및 과제 작업물을 Chat GPT를 이용하여 정리하는 저장소입니다.

<img width="1277" height="736" alt="제목 없음" src="https://github.com/user-attachments/assets/b28f2218-516d-4f55-b231-67db978eeade" />

## 목적

- 수업에서 구현한 기능과 예제를 주차별 또는 주제별로 관리
- 소스 코드와 필요한 프로젝트 파일을 Git으로 버전 관리

## 2026-09-07

- **DirectX 11 Framework**  
  Window, Renderer, Input, Camera, Mesh, Shader, Constant Buffer를 역할별로 분리하고 초기화 오류 진단 기능을 추가했습니다.

- **Perlin Noise Terrain**  
  Seed 기반 Perlin Noise로 Terrain 높이를 생성하고 Grid Mesh, UV, Smooth Normal을 구성했습니다.

- **HeightMap Terrain**  
  WIC로 HeightMap 이미지를 읽고 픽셀 밝기를 높이값으로 변환하여 Terrain을 생성했습니다.

- **Texture Splatting**  
  Terrain의 높이와 경사도를 기준으로 Grass / Rock / Snow Texture Weight를 자동 계산하고 자연스럽게 혼합하도록 구현했습니다.

- **QuadTree Frustum Culling**  
  Terrain을 QuadTree로 분할하고 Camera Frustum 밖의 영역을 렌더링에서 제외하도록 구현했습니다. `F1`로 Culling ON/OFF를 전환하고 화면에서 Visible Leaves, Rendered Triangles 등의 통계를 확인할 수 있습니다.

## 2026-09-14

- **QuadTree LOD**  
  Camera와 Terrain 영역 사이의 거리를 기준으로 QuadTree Node의 Detail Level을 선택하여, 가까운 영역은 높은 해상도로 유지하고 먼 영역은 적은 Triangle로 렌더링하도록 구현했습니다. `F2`로 LOD ON/OFF를 전환하고 Surface Triangles와 LOD별 Node 통계를 확인할 수 있습니다.

- **Showcase Visibility Toggle**  
  누적된 테스트 오브젝트와 Terrain을 개별적으로 확인할 수 있도록 숫자키 `1~0` 기반 표시 ON/OFF 기능을 추가했습니다. Cube, Perlin Terrain, HeightMap Terrain, QuadTree LOD Terrain, Triplanar Terrain을 각각 분리해서 확인할 수 있습니다.

- **Triplanar Mapping**  
  Terrain의 World Position과 World Normal을 기준으로 X / Y / Z 방향에서 Texture를 투영하고 혼합하여, 절벽과 같이 경사가 큰 영역에서 발생하는 Texture Stretching을 줄이도록 구현했습니다. 기존 Height / Slope 기반 Grass / Rock / Snow Texture Splatting과 QuadTree LOD 구조는 그대로 유지합니다.

## 2026-09-21

- **Terrain Chunk 분할 및 표면 경계선** — [TerrainChunk v0.3](DXFramework_TerrainChunk_v0.3_SurfaceBorders_VisualStudio/)  
  129×129 HeightMap을 32×32 Cell 단위의 16개 Chunk로 분리하고, Chunk별 Mesh·Bounds·QuadTree LOD·Frustum Culling을 적용했습니다. 경계선은 AABB 상단이 아닌 실제 지형 표면 높이를 따라 그리도록 변경했습니다.

- **GPU Chunk Streaming 및 회전형 미니맵** — [TerrainStreaming v0.5](DXFramework_TerrainStreaming_v0.5_RotatingCompass_VisualStudio/)  
  카메라 타겟과 Chunk AABB 사이의 XZ 거리를 기준으로 필요한 GPU Mesh만 생성·해제하도록 구성했습니다. Load/Unload 반경에 히스테리시스를 적용하고, 로딩 상태를 보여주는 2D 미니맵에 카메라 방향 연동 회전과 N/E/S/W 나침반을 추가했습니다. 이 버전은 전체 HeightMap의 CPU 데이터를 유지하는 GPU 중심 Streaming입니다.

- **대형 지형 Disk-based Tile Streaming** — [DiskTerrainStreaming v0.2](DXFramework_DiskTerrainStreaming_v0.2_ZoomAdaptiveRadius_VisualStudio/)  
  새 프로젝트에서 기존 Showcase의 초기화·렌더링을 제외하고, 1024×1024 Cell 지형을 64×64 Cell 크기의 256개 Tile 파일로 구성했습니다. 시작 시에는 Metadata만 읽고, 필요한 Tile을 Worker에서 비동기로 읽어 CPU Mesh와 QuadTree LOD를 준비한 뒤 메인 스레드에서 GPU에 업로드합니다. 인접 Tile의 경계 높이·Normal 처리를 위해 Halo Sample을 사용합니다. 해당 v0.2에서는 카메라 Zoom에 따라 로딩 반경을 바꾸는 기능을 실험했습니다.

- **직접 구현한 Custom UI Framework** — [Numeric Input v0.5](DXFramework_DiskTerrainStreaming_v0.5_NumericInput_VisualStudio/)  
  Dear ImGui 없이 Direct2D·DirectWrite를 이용해 공통 UI Renderer, Widget, Manager 및 Terrain 설정 패널을 구현했습니다. 기존 F1~F6의 Culling·LOD·통계·경계선·Streaming·미니맵 토글을 체크박스로 옮기고, Load Radius를 슬라이더로 조절하도록 변경했습니다. 현재 구성에서는 마우스 휠을 카메라 확대·축소 전용으로 사용하며 로딩 반경과 분리합니다.

- **미니맵 배율 및 숫자 직접 입력** — [MiniMap Zoom v0.4](DXFramework_DiskTerrainStreaming_v0.4_MiniMapZoom_VisualStudio/) · [Numeric Input v0.5](DXFramework_DiskTerrainStreaming_v0.5_NumericInput_VisualStudio/)  
  미니맵 패널 크기와 실제 로딩 범위는 유지하면서 내부 격자·반경선의 표시 배율만 50~400%로 조절하는 슬라이더를 추가했습니다. Load Radius 및 MiniMap Zoom 값은 숫자 입력칸에서도 직접 수정할 수 있으며, Enter 적용·Esc 취소·입력 범위 제한과 편집 중 WASD 이동 차단을 구현했습니다.

## 2026-09-22

- **Predictive Prefetch 및 CPU LRU Cache** — [DiskTerrainStreaming v0.7](DXFramework_DiskTerrainStreaming_v0.7_LoadMetrics_VisualStudio/)  
  카메라 이동 방향의 Tile을 미리 읽는 Predictive Prefetch와 Tile 높이 데이터를 재사용하는 용량 제한 CPU LRU Cache를 구현했습니다. 필수 Tile 요청을 우선 처리하고, UI에서 Prefetch·Cache 활성화 여부, 선행 거리 및 Cache 용량을 조절할 수 있도록 했습니다. GPU Mesh는 기존처럼 필요 범위를 벗어나면 해제합니다.

- **Streaming 성능 계측** — [Load Metrics v0.7](DXFramework_DiskTerrainStreaming_v0.7_LoadMetrics_VisualStudio/)  
  Cache Hit/Miss, Disk Read Time(최근·평균·누적), Tile Load Time(최근·평균·누적), 완료된 Tile Load 수를 Debug 통계에 추가했습니다. Tile Load Time은 필수 요청부터 GPU 버퍼 생성 및 설치 완료까지의 경과시간이며 실제 GPU Draw 완료 시간은 포함하지 않습니다. 사용자가 Windows 환경에서 기능 동작을 확인했으며, 성능 개선 폭에 대한 동일 경로 ON/OFF 비교 측정은 아직 진행하지 않았습니다.

- **접이식 설정창 및 반투명 통계창** — [Collapsible UI v0.8.1](DXFramework_DiskTerrainStreaming_v0.8.1_TranslucentStats_VisualStudio/)  
  설정창을 기본적으로 닫아두고 우측 버튼으로 열고 닫으며, 제목줄 드래그로 이동할 수 있도록 변경했습니다. UI 문구를 한글 중심으로 정리하되 기술 용어는 영어를 유지했습니다. 통계창 배경의 불투명도를 0.68로 낮추고 글자와 테두리는 그대로 표시하도록 했습니다.

- **재사용 가능한 Custom UI Framework v0.2** — [DiskTerrainStreaming v0.9](DXFramework_DiskTerrainStreaming_v0.9_UIFramework_v0.2_VisualStudio/)  
  공통 패널 등록 API, 위젯 높이 기반 레이아웃, 다중 패널 입력·포커스 관리, UI Theme 및 패널 본문 Clip을 정리했습니다. Terrain에 의존하지 않는 테스트 패널에서 Checkbox·Slider·Button·숫자 입력을 확인하고 기존 Terrain 설정 기능을 유지했습니다. 사용자가 실제 실행 테스트를 완료했습니다. 이 단계 프로젝트의 README는 `README.md` 한 개만 유지합니다.

각 단계의 세부 구현 내용과 빌드·테스트 방법은 해당 프로젝트 폴더의 `README.md`에 정리합니다.
