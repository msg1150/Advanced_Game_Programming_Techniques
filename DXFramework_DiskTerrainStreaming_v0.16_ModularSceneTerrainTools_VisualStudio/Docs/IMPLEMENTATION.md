# v0.16 구현 기록

## 원본 보존

v0.15를 별도 v0.16 폴더로 복제했습니다. 복제된 .vs/bin/obj/.vcxproj.user는 이전 경로를 참조하는 개인 설정·생성물이므로 복제본에서만 제외했습니다. 이전 버전은 삭제·수정하지 않았습니다. 새 Project GUID와 빌드 설명 파일을 유지합니다.

## 작업 순서

1. FeatureConfig, MSBuild props, CMake 옵션 도입
2. Picking 전용 Depth, Pipeline 바인딩 복원, 이전 결과 무효화
3. 순수 Scene 문서/스트림 직렬화 및 기존 UTF-8 변환 재사용
4. 기존 World의 ID 할당 상태를 이어서 후보 World를 만들고 교체하는 Adapter
5. Scene 파일 UI와 Win32 파일 선택·교체 어댑터
6. 기존 Tile 포맷/Cache를 재사용하는 ITerrainTileSource, Disk 공급자, TerrainQuery
7. 기존 Cell 대각선 분할을 따르는 높이·법선 조회와 XZ DDA Raycast
8. 순수 배치 정책과 Input/Camera/UI 어댑터 분리
9. 기존 테스트 CTest 등록, 새 회귀 테스트, MSVC 자동 탐색 테스트 스크립트
10. 전체/기능별 제외/전체 제외/실제 파일 제거 후 빌드 검증

## 재사용과 의존성

- Collider/Shape/World, World Bounds는 기존 Collision Core를 사용합니다.
- 공통 UIManager/Widget에는 새 Feature 전용 데이터를 추가하지 않습니다.
- 기존 UIUtf8의 순수 UTF-8 변환 함수를 사용합니다.
- 기존 TileArchive와 TileSampleCache를 사용합니다. DiskTerrainManager와 Worker는 변경하지 않습니다.
- Query는 Scene 편집기를 모르며 직렬화는 파일 대화상자를 호출하지 않습니다.
- 연결은 Application과 Feature별 어댑터에 한정합니다.
- 경로는 사용자가 선택하고 공급자/거리/간격/Cache 용량은 생성자 또는 Initialize 인자로 전달합니다.
- 제외 시 빌드 대상과 헤더 참조를 함께 제외합니다.

## Scene 포맷

UTF-8 text이며 magic은 DXCOLLIDER_SCENE, 현재 version은 1입니다.

```text
DXCOLLIDER_SCENE 1 1
Sphere "Example" 0 5 0 0 0 0 1 4294967295 1 0 2
```

레코드는 Shape, quoted name, Position XYZ, Rotation XYZ(radians), Layer, Mask, Enabled, Trigger, Shape parameters 순서입니다. Box는 Half Extents XYZ, Capsule은 Radius/HalfSegment, Sphere는 Radius를 기록합니다. float는 max_digits10과 classic locale을 사용합니다.

UI 파일 크기 64 MiB, Collider 100,000개, UTF-8 이름 4,096 bytes를 제한합니다. 유한한 수·양수 크기·Bool 범위·지원 Shape·문서 끝을 검사합니다. 읽기 실패 시 출력 문서와 현재 World를 유지합니다. Runtime Adapter에는 검증된 문서를 전달합니다. 포맷 변경 시 Version과 마이그레이션 정책을 함께 확장하세요.

## 확장 접점과 범위

- Gameplay Query는 매 프레임 Disk I/O 대신 상주/비동기 공급자로 교체합니다.
- 지면 배치는 중심과 World AABB 하단 기준이며 경사면 전체 침투 해소 기능이 아닙니다.
- Camera/UI/Terrain 설정은 별도 문서 또는 모듈별 직렬화로 확장할 수 있습니다.
- MSAA 추가 시 Picking/Depth 복사 정책을 확장해야 합니다.
- UI 콜백과 Feature의 수명은 Application에 묶여 있습니다. 실행 중 동적 해제에는 UI 등록 해제 API와 수명 관리가 먼저 필요합니다.
- Rigidbody/Solver, Trigger 이벤트, Undo/Redo, Gizmo는 이번 단계에 포함하지 않습니다.
