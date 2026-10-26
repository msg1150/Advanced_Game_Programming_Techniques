# v0.16 검증 기록 — 2026-10-26

## 원본 보존

- 기존 Git 추적 파일 변경: 없음 (`git diff --exit-code` 성공).
- v0.15와 v0.16의 Source/Shaders/Assets/Tests/Tools를 SHA-256으로 비교했습니다.
- 복제한 기존 파일 393개가 동일합니다. 복제본에서 변경한 기존 소스는 7개입니다.
  - Core/Application.h, Application.cpp
  - Features/CollisionEditor/CollisionScene.h, CollisionScene.cpp, CollisionEditorPanel.cpp
  - Features/GPUPicking/GPUPickingSystem.h, GPUPickingSystem.cpp
- 기존 Physics Core, Terrain Manager/Worker, 공통 UI와 모든 기존 테스트 및 Asset은 동일합니다.

## 빌드

Windows SDK 10.0.26100.0, MSVC v143(19.44) 및 v145(19.51)로 확인했습니다.

| 구성 | 결과 |
|---|---|
| v143, Debug x64, 전체 기능 | 통과 |
| v145, Debug x64, 전체 기능 | 통과 |
| Picking만 제외 | 통과 |
| Scene Persistence만 제외 | 통과 |
| Placement만 제외 | 통과 |
| Terrain Query 제외 → Placement 자동 제외 | 통과 |
| 새 모듈 전체 제외 | 통과 |
| 별도 사본에서 새 모듈 소스/헤더/Shader를 실제 제거 | 통과 |
| CMake 생성 프로젝트 전체 빌드 (MSBuild 2022, 순차 빌드) | 통과 |

실제 파일 제거 검증은 원본 및 작업 중인 v0.16 밖의 별도 사본에서 수행했습니다. GPUPicking, ScenePersistence, Scene/, Terrain/, TerrainPlacement, SceneFileDialogs.* 및 Picking Shader를 제거한 뒤 전체 옵션을 끄고 링크까지 확인했습니다. 이때 Placement=true를 명시해도 Query=false이면 유효 Placement가 제외되는 구성도 확인했습니다.

기능별 제외 검증은 MSBuild `/p:EnableGPUPicking=false`, `/p:EnableScenePersistence=false`, `/p:EnableTerrainPlacement=false`, `/p:EnableTerrainQuery=false`를 각각 적용했습니다. 모든 빌드는 런타임 실행 없이 컴파일·링크만 수행했습니다.

### 검증 환경의 도구 문제와 적용한 우회

설치된 v143 PDB 도구에서 MSPDB140.DLL 버전 충돌을 확인했습니다. 시스템 설치를 변경하지 않고 검증 프로세스에만 `_CL_=/Z7`, `_LINK_=/DEBUG:NONE /INCREMENTAL:NO`를 적용하여 컴파일·링크했습니다. 기본 프로젝트의 Debug 심볼 설정은 보존했습니다. 이 검증은 일반 Debug PDB 생성 성공을 뜻하지 않습니다.

CMake는 긴 버전 폴더 아래의 CompilerId 경로에서 Windows 경로 길이 문제를 확인하여 작업 디렉터리 밖의 짧은 빌드 경로를 사용했습니다. 이 환경에서는 CMake가 선택한 최신 MSBuild 대신 MSBuild 2022로 생성된 프로젝트를 직접 빌드했습니다. 기본 프로젝트 소스 변경으로 도구 설치 문제를 숨기지 않았습니다.

## 자동 테스트

MSVC 자동 탐색 `Tools/Test.ps1`로 다음 21개 실행 파일의 통과를 확인했습니다. 실패한 테스트만 수정 후 다시 실행했습니다.

- 기존 18개: Collision Shapes/World/Raycast, Tile Archive, Streaming Policy/Timing, Prefetch Cache, Geomorph Policy, Numeric Value, UI Layout/Placement/Scroll/Interaction, Binding Registry, XML, Factory, Designer Model
- test_scene_document: 4종 Shape, UTF-8/인용부호/개행 이름, Transform, Layer/Mask, Enabled/Trigger, 재로드 ID 비재사용, 잘못된 버전/수치/Shape/Bool/끝 데이터 거부, 실패 시 기존 데이터 보존
- test_terrain_query: 평면 삼각형 보간, 두 Cell 삼각형, Tile 경계/부분 Tile/음수 Origin, 수직·대각선 Ray, 거리 제한, Miss/Invalid/Unavailable, OBB 바닥 배치 및 실제 Terrain Archive 연동
- test_gpu_picking: 창 없는 WARP 장치, 실제 HLSL Compile, 두 그리기 순서에서 최근접 선택, Depth/World Position, 지형 가림, 빈 Scene, 원본 Depth 보존, Pipeline 바인딩 복원, 이전 Readback 무효화

자동 GPU 테스트는 런타임 앱을 열지 않으며 실제 GPU 드라이버 및 사용자 입력에 대한 플레이 테스트를 대체하지 않습니다.

최종 CTest 실행도 **22/22 통과, 실패 0개**로 완료했습니다. 기존 18개, 새 3개 및 별도로 등록한 실제 Terrain Archive 연동 Query 실행을 포함합니다. CMake 테스트 대상에 NOMINMAX/WIN32_LEAN_AND_MEAN을 적용해 Windows min/max 매크로와 기존 UI 수학 코드의 충돌을 방지했습니다. 기존 UI 소스는 변경하지 않았습니다.

## 사용자 플레이 테스트 항목

1. 새 버전의 DXFramework.sln으로 빌드·실행하고 Terrain/Streaming/LOD/Geomorph/MiniMap/UI Designer가 유지되는지 확인합니다.
2. Collider 두 개를 화면에서 겹치게 배치하고 앞 물체가 선택되는지 확인합니다. 생성 순서를 바꾸어도 결과가 같아야 합니다.
3. 지형 뒤 Collider는 선택되지 않아야 합니다. 창 크기 변경 후에도 클릭 위치와 선택이 맞아야 합니다.
4. Collider의 위치·회전·크기·Enabled·Trigger를 바꾸고 Scene을 저장합니다. 장면을 지운 뒤 불러오면 설정이 복원되어야 합니다. 선택은 해제됩니다.
5. 파일 대화상자 취소 시 장면이 유지되는지, 잘못된 버전/손상된 파일은 오류 상태만 표시하고 기존 장면을 유지하는지 확인합니다.
6. Sphere/AABB/회전 OBB/Capsule을 선택하여 지형 클릭 배치 및 현재 X/Z 배치를 확인합니다. 간격과 바닥 기준 옵션이 반영되어야 합니다.
7. 배치 중 UI 클릭/스크롤이 배치로 새지 않고 Orbit이 차단되는지, Escape 해제 후 기존 Picking/Orbit이 복원되는지 확인합니다.
8. 지형 밖 클릭이나 조회 불가 시 기존 위치가 유지되어야 합니다. 거친 LOD에서 원본 높이와 화면의 차이는 위의 기능 범위를 참고합니다.
9. FeatureOptions.props에서 옵션을 끈 재빌드로 해당 UI가 사라지고 나머지 기능이 유지되는지 확인합니다. Picking이 없는 경우 다음 Collider 선택을 사용합니다.

플레이 테스트는 사용자 요청에 따라 수행하지 않았습니다.
