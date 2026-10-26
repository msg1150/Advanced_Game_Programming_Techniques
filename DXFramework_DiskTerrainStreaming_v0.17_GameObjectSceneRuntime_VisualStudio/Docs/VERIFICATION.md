# v0.17 검증 기록 — 2026-10-26

## 검증 범위

이전 작업에서 검증을 과하게 반복했던 점을 반영하여, 이번에는 전체 빌드와 새 기능 테스트 및 필요한 제외 구성으로 한정했습니다. 런타임 앱과 플레이 테스트는 실행하지 않았습니다.

| 검사 | 결과 |
|---|---|
| Visual Studio 2022 / MSVC v143 / Debug x64 전체 기능 컴파일·링크 | 통과 |
| EnableEngineRuntime=false (기존 v0.16 기능만 유지) | 통과 |
| EngineGraphics/EngineCollision/EngineGameplay=false (Scene 코어만 유지) | 통과 |
| test_engine_runtime | 통과 |
| test_engine_graphics (창 없는 WARP) | 통과 |
| test_scene_document (기존 Collider 저장 회귀) | 통과 |
| 기존 Git 추적 파일 변경 여부 | 변경 없음; v0.17 폴더만 추가 |

새 모듈의 모든 조합을 반복 빌드하거나 기존 22개 테스트 전체를 재실행하지는 않았습니다. CMake에는 새 테스트를 등록했으며 이번 실행 경로는 MSBuild 프로젝트와 Tools/Test.ps1입니다.

## 새 테스트 내용

### test_engine_runtime

- Attach/Start/FixedUpdate/Update/Stop/Detach 및 Component 제거
- 활성·비활성 전환과 재시작
- 부모/자식 World Transform 및 World Position의 Local 변환
- 순환 Parent 거부, 부모/자식 삭제, 삭제 ID 비재사용
- Scene Token이 다른 과거 핸들의 무효화
- Registry 기반 Component/Transform/Parent/ObjectLink/Asset 경로 저장·복원
- 알려지지 않은 Component의 속성 보존 후 모듈 복구
- 손상/버전/순환/끝 데이터 거부 및 현재 Scene 유지
- Scene 전환 요청의 경계 적용, Stop
- ResourceCache 중복 로드 방지, 공유 수명, 미사용 회수, 실패 캐시·무효화
- 고정 Step Movement, 장애물 이동 차단, Trigger Enter/Stay/Exit
- GroundProvider 접지 및 조회 실패 시 위치 유지
- 실제 Runtime.engine.scene의 객체/계층/참조 로드

### test_engine_graphics

- WARP Device 및 창 없는 Render Target 생성
- 실제 Cube.mesh와 Scene 파일 로드
- ObjectVS/ObjectPS의 실제 HLSL Compile
- 여러 객체가 동일 Mesh/Shader를 공유하는 캐시 수량 확인
- 실제 Grass.png의 Texture2D/WIC 로드와 자원 캐시
- 변경한 Pipeline 바인딩 복원
- 없는 Asset의 오류 상태 및 명시적 Cache 초기화

## 검증 환경

설치된 PDB 도구 충돌은 이전 단계와 동일하여, 빌드 프로세스에만 `_CL_=/Z7`, `_LINK_=/DEBUG:NONE /INCREMENTAL:NO`를 적용했습니다. 시스템 설치나 기본 프로젝트의 Debug 심볼 설정은 변경하지 않았습니다. 따라서 위 결과는 일반 PDB 생성까지 검증했다는 의미가 아닙니다.

검증 출력은 작업용 외부 디렉터리 dx17-build, dx17-modules, dx17-tests에 두었고 저장소에는 포함하지 않습니다. 기존 Terrain/Physics/UI 소스에 새 엔진 의존성을 넣지 않았습니다.

## 사용자 플레이 테스트

1. Engine 열기 → 모두 지면에 맞추기 → Play → WASD 이동을 확인합니다.
2. 장애물에 막히고 Trigger를 통과하는지, 자식 Marker가 Player를 따라오는지 확인합니다.
3. Pause 상태에서 공통 Transform을 편집하고 Engine Collider 표시와 Mesh가 함께 변하는지 확인합니다.
4. Component를 활성/비활성·제거·재부착하고 다른 객체가 유지되는지 확인합니다.
5. 빈 객체 생성/복제/삭제 및 부모 삭제 시 자식 삭제를 확인합니다.
6. Engine Scene 저장 → 수정/삭제 → 재진입 또는 불러오기로 상태·부모·참조가 복원되는지 확인합니다.
7. 손상된 파일/기존 Collider 전용 파일을 Engine Scene으로 열 때 현재 장면이 보존되는지 확인합니다.
8. 기존 Terrain/UI Designer/Collider 편집·저장·배치가 유지되는지 확인합니다.

원형 Collider의 비균일 Scale, Shear의 보수적 Bounds, 동기 Ground Query, 고속 이동/얇은 벽 제한은 README를 참고하세요. Engine Object 선택은 Engine 패널에서 수행하며 기존 GPU Picking의 대상은 v0.16 Collider World입니다.
