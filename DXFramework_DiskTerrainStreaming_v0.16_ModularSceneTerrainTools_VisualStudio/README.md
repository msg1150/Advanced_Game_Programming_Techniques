# DirectX 11 Framework v0.16 — Modular Scene / Terrain Tools

v0.15 CollisionGPUPicking을 **별도 폴더로 복제**한 버전입니다. 이전 버전 폴더는 수정하지 않았습니다.

- 베이스: b7c775b (2026-10-12)
- 작업일: 2026-10-26
- 기존 Terrain/Streaming/LOD/Geomorph/UI/Designer/Collision 코드를 재사용합니다.
- 플레이 테스트는 사용자가 수행합니다. 자동 검증에서 런타임 앱은 실행하지 않습니다.

## 추가 기능

### GPU Picking 정확성

Scene Depth를 Picking 전용 Depth Buffer에 복사하고, Proxy끼리 Depth를 기록·비교하여 가장 가까운 표면을 선택합니다. 생성/그리기 순서에 의존하지 않고 원본 Scene Depth는 보존합니다. 현재 Renderer의 단일 Sample Depth를 지원합니다.

기존 1×1 Readback, 3-slot Ring, 클릭 당시 행렬을 이용한 World Position 복원을 유지합니다. 생성·삭제·수동 선택·Scene 불러오기·모드 전환 시 이전 결과를 무효화하되 GPU 사용 중인 Slot은 완료 후 회수합니다. 변경한 RT, Depth/Blend/Rasterizer, Viewport/Scissor, IA, VS/PS와 사용 Constant Buffer 바인딩을 복원합니다.

### Collider Scene 저장·불러오기

상단 **Scene 파일 열기** → **Scene 저장... / Scene 불러오기...**에서 경로를 선택합니다.

- 저장: Shape/크기, UTF-8 이름, Position/Rotation, Layer/Mask, Enabled/Trigger
- 제외: Camera, Terrain 설정, UI 배치, GPU 상태
- 기존 UI XML과 별도의 버전이 있는 `.scene` 포맷이며 고정 저장 경로가 없습니다.
- 파일 전체와 수치·문서 버전·UTF-8 등을 검증하고 후보 World를 구성한 뒤 교체합니다. 실패하면 기존 장면을 유지합니다.
- Runtime ID는 저장하지 않고 기존 World의 ID 할당 상태를 이어서 새 ID를 발급합니다.
- 저장은 같은 디렉터리의 임시 파일을 완전히 쓴 뒤 대상 파일을 교체합니다.

### Terrain Query와 지면 배치

1. Collision 패널에서 Collider를 생성/선택합니다.
2. **지면 배치 열기** → **선택 Collider 지면 배치 모드**를 켭니다.
3. UI 밖의 지형을 클릭하면 선택 Collider가 이동합니다.
4. **현재 X/Z에서 지면에 놓기**로 높이 조회만 수행할 수도 있습니다.
5. **Collider 바닥 기준 배치**, **지면과 간격**을 조절합니다. Escape로 모드를 해제합니다.

Picking 제외 빌드에서는 **다음 Collider 선택**으로 기존 물체를 선택할 수 있습니다. 배치 중에는 Scene 클릭과 Camera Orbit을 도구가 소유하여 Picking/Orbit으로 중복 전달하지 않습니다. UIManager의 입력 처리를 우선합니다.

TerrainQuery는 원본 Cell 삼각형 기준 높이·법선 조회 및 Raycast를 제공합니다. 기존 대각선 분할을 사용하며 XZ Grid DDA로 Ray가 지나는 Cell만 검사합니다. 렌더링 LOD/Geomorph와 GPU Tile 상주 여부에 의존하지 않습니다.

공급자는 ITerrainTileSource로 주입합니다. Editor Disk 공급자는 기존 TileArchive와 TileSampleCache를 재사용하고 **조회 버튼/클릭 시에만 동기 Disk I/O**를 수행합니다. 렌더링 Worker의 Cache/Queue는 수정하지 않습니다. Gameplay의 매 프레임 조회는 상주/비동기 공급자로 교체하세요. Tile을 제공하지 못하면 Unavailable을 반환하고 물체 위치를 유지합니다.

지면 배치는 중심 XZ의 높이와 Collider의 World AABB 하단을 맞추는 Editor 기능입니다. 경사면 전체 침투 해소, Rigidbody, 중력, 충돌 반응, HeightField Solver는 포함하지 않습니다. 거친 LOD/Geomorph 화면과 원본 표면 사이에는 시각적 차이가 있을 수 있습니다.

## 모듈 포함·제외

| 기능 | Source 기준 독립 코드 | MSBuild 속성 | CMake 옵션 |
|---|---|---|---|
| Picking | Features/GPUPicking | EnableGPUPicking | DX_ENABLE_GPU_PICKING |
| Scene 파일 | Scene/, Features/ScenePersistence, Platform/SceneFileDialogs.* | EnableScenePersistence | DX_ENABLE_SCENE_PERSISTENCE |
| 지형 조회 | Terrain/Query | EnableTerrainQuery | DX_ENABLE_TERRAIN_QUERY |
| 지면 배치 | Features/TerrainPlacement | EnableTerrainPlacement | DX_ENABLE_TERRAIN_PLACEMENT |

기본값은 모두 활성화입니다. Query를 제외하면 Placement도 자동 제외합니다. 저장/Picking은 Query에 의존하지 않고 Placement는 Picking에 의존하지 않습니다. 공통 FeatureConfig.h가 빌드 설정을 전달합니다.

**빌드 시 포함·제외 옵션입니다. 실행 중 DLL 언로드 기능은 아닙니다.** 옵션을 끄고 재빌드하면 해당 UI·소스·헤더 참조가 제외됩니다. 파일을 실제 제거할 때도 먼저 옵션을 끄세요. Query 제거 시 Terrain/Query와 Features/TerrainPlacement를 함께 제거할 수 있습니다. 공통 Collision/Tile 타입과 FeatureConfig는 유지합니다.

기존 Physics Core, DiskTerrainManager, UIManager는 새 저장/조회/배치 모듈을 참조하지 않습니다. Application이 기능들을 조합합니다. 선택 기능 초기화 실패 시 로그를 남기고 해당 기능을 제외합니다. UI 콜백을 보유한 Feature는 UI와 같은 Application 수명을 사용합니다.

## 빌드 및 자동 테스트

Visual Studio 2022에서 DXFramework.sln → Debug | x64로 빌드합니다. 복제본은 이전 버전과 다른 Project GUID를 사용합니다. FeatureOptions.props의 기본값을 수정하거나 MSBuild `/p:EnableGPUPicking=false` 같은 속성을 사용합니다.

Developer PowerShell에서 CMake를 사용할 수 있습니다. Windows 경로 길이 제한을 피하도록 짧은 빌드 경로를 권장합니다.

```powershell
cmake -S . -B C:/temp/dx16-build -G "Visual Studio 17 2022" -A x64
cmake --build C:/temp/dx16-build --config Debug --parallel 4
ctest --test-dir C:/temp/dx16-build -C Debug --output-on-failure
# 기능 제외 예: -DDX_ENABLE_GPU_PICKING=OFF
```

MSVC 자동 탐색 테스트 스크립트도 제공합니다.

```powershell
./Tools/Test.ps1
./Tools/Test.ps1 -Filter test_scene_document
```

기존 18개 테스트와 Scene 문서, Terrain Query, 창 없는 WARP Picking 테스트를 제공합니다. WARP는 HLSL Compile, 겹침 순서, 지형 가림, Depth 보존, 이전 결과 무효화를 검사하는 자동 테스트이며 플레이 테스트와 별개입니다. CTest에는 실제 Terrain Archive 연동 Query 실행도 추가 등록됩니다.

작업 과정: [Docs/IMPLEMENTATION.md](Docs/IMPLEMENTATION.md)

검증 결과와 사용자 수동 확인 항목: [Docs/VERIFICATION.md](Docs/VERIFICATION.md)
