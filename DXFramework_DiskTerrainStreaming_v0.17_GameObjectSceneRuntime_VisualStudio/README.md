# DXFramework v0.17 — GameObject / Component / Scene Runtime

v0.16을 별도 폴더로 복제하여 만든 버전입니다. v0.16과 이전 버전은 수정하지 않았습니다. 베이스는 main c6f33aa이며 작업일은 2026-10-26입니다.

## 이번 버전의 범위

- GameObject와 독립 Component, 활성/비활성, 안전한 지연 삭제
- 공통 Local TRS와 부모/자식 World Transform, 순환 관계 차단
- OnAttach → OnStart → FixedUpdate/Update → OnStop → OnDetach 수명 관리
- SceneManager의 경계 시점 Scene 교체와 재진입
- GameObject/Component/부모/참조/활성 상태와 Asset 경로를 저장하는 Engine Scene
- Mesh/Texture/Shader 공유 ResourceCache 및 기존 DirectX 자원 클래스 어댑터
- 공통 Transform에서 Collider를 생성하는 CollisionBridge
- 접촉 Enter/Stay/Exit, Trigger 이벤트, 고정 60Hz 입력 이동과 간단한 장애물 차단
- 원본 Terrain 높이 공급자를 주입한 지면 접지

기존 Terrain Streaming, LOD/Geomorph, UI Designer, Collider 편집/저장/배치는 그대로 유지합니다. 새 Engine Scene은 별도 World를 소유하므로 기존 도구의 상태를 강제로 바꾸지 않습니다.

## 직접 확인하는 순서

1. DXFramework.sln을 Debug | x64로 빌드하고 실행합니다.
2. 상단 **Engine 열기**로 패널을 엽니다. 샘플 Scene은 기본 Pause이며 Assets/Scenes/Runtime.engine.scene에서 읽습니다.
3. **모두 지면에 맞추기**를 누른 뒤 **Play**를 켭니다. WASD는 Movement가 붙은 물체에 전달되고 Camera WASD는 차단됩니다. UI 숫자 편집 중에는 이동 입력을 차단합니다.
4. Player가 이동하고, 자식 Child Marker가 따라오는지 확인합니다. 장애물에 막히며 Trigger는 통과해야 합니다. **Engine Collider / Contact 보기**로 Collider를 확인합니다.
5. **다음 GameObject 선택**으로 선택한 뒤 Position/Rotation/Scale을 편집합니다. Mesh와 Collider가 같은 World Transform을 사용합니다. 현재 선택은 패널 기반이며 기존 GPU Picking은 v0.16 Collider 편집 World를 대상으로 합니다.
6. **다음 Component**, 활성 체크, 제거, Collider/Movement/MeshRenderer 붙이기를 사용합니다. MeshRenderer를 붙일 때 .mesh 파일을 선택합니다. 같은 타입의 중복 부착은 상태 메시지로 거부합니다.
7. 빈 오브젝트 생성, 선택 복제, 삭제를 확인합니다. 부모 삭제는 자식도 삭제합니다. 복제는 선택 객체 한 개이며 자식 전체 복제는 하지 않습니다.
8. **Engine Scene 저장...** 후 수정하고 **현재 Scene 재진입** 또는 **Engine Scene 불러오기...**를 사용합니다. 객체·Component·Transform·부모·ObjectLink·Asset 경로가 복원됩니다.

Play 해제는 현재 상태에서 일시 정지합니다. 파일 상태로 복원하려면 재진입을 사용합니다. 기존 **Scene 파일 열기**는 Collider 전용 포맷이고 Engine 패널의 버튼은 DXENGINE_SCENE 포맷입니다. 잘못된 포맷/손상된 파일을 열면 기존 Scene을 유지하고 오류를 표시합니다.

## 코드 경계

| 계층 | 경로 | 책임 |
|---|---|---|
| 공통 실행 | Source/Engine/Core | 객체, Transform, Component 수명, 고정 Step, Scene 전환 |
| 문서 | Source/Engine/Serialization | Registry 기반 저장/복원, 참조 검증 |
| 자원 | Source/Engine/Resources | 타입별 공유 캐시, 실패 캐시, 명시적 무효화 |
| 렌더 어댑터 | Source/Engine/Graphics | 기존 Mesh/Texture2D/Shader 재사용 |
| 충돌 어댑터 | Source/Engine/Collision | 기존 CollisionWorld/Detector 재사용, 접촉 이벤트 |
| 게임플레이 | Source/Engine/Gameplay | Movement Component, 주입 가능한 이동 함수 |
| 입력/UI 조립 | Source/Features/EngineRuntime | Win32/입력/파일 선택/기존 UI 연결 |

코어는 Win32·DirectX·UI·렌더 어댑터에 의존하지 않습니다. 기존의 순수 CollisionMath를 재사용합니다. ComponentRegistry에 Factory를 등록하여 타입을 확장하며 직렬화는 특정 Component 타입을 분기하지 않습니다. 객체 참조를 가진 새 Component는 References/RemapReferences를 구현합니다.

## 모듈 제외

FeatureOptions.props 또는 MSBuild /p 속성으로 설정합니다.

| MSBuild | CMake |
|---|---|
| EnableEngineRuntime | DX_ENABLE_ENGINE_RUNTIME |
| EnableEngineGraphics | DX_ENABLE_ENGINE_GRAPHICS |
| EnableEngineCollision | DX_ENABLE_ENGINE_COLLISION |
| EnableEngineGameplay | DX_ENABLE_ENGINE_GAMEPLAY |

기본값은 모두 true/ON입니다. Runtime을 끄면 하위 어댑터도 제외합니다. 하위 모듈을 개별 제외해도 Scene 문서는 누락 Component의 타입·속성을 MissingComponent로 보존하여 다시 저장할 수 있습니다. 모듈을 복원하고 재로드하면 원래 Component로 복원합니다. Collision을 제외한 Movement는 주입된 기본 이동 경로를 사용하며 장애물 차단은 하지 않습니다.

옵션은 빌드 시 포함/제외이며 실행 중 DLL 언로드가 아닙니다. UI에서 Component 제거/활성 변경은 실행 중 지원합니다. 코어만 유지하려면 Graphics/Collision/Gameplay를 모두 끄면 됩니다. 전체 제거 시 옵션을 끄고 Source/Engine 및 Source/Features/EngineRuntime을 제거할 수 있습니다. 기존 v0.16 옵션도 유지됩니다.

## Asset과 한계

- 샘플 객체/Component/색상/크기/속도/참조는 Runtime.engine.scene에 정의합니다. C++에서 샘플 객체를 생성하지 않습니다.
- .mesh는 버전이 있는 정적 Vertex/Index 텍스트 포맷입니다. FBX/glTF/애니메이션 Importer는 포함하지 않습니다.
- 자원 키는 정규화한 UTF-8 경로와 로딩 옵션입니다. 성공/실패를 캐시하여 매 프레임 재로딩하지 않습니다. 패널의 캐시 재로드로 명시적으로 재시도합니다.
- 접지는 기존 TileArchive/Cache 기반 공급자를 재사용하므로 새 Tile 첫 조회 시 동기 I/O가 발생할 수 있습니다. 실시간 대규모 게임은 상주/비동기 GroundProvider로 교체해야 합니다.
- 이동은 고정 Step의 축별 이동/차단입니다. Rigidbody, 중력, Impulse Solver, CCD, 계단·경사면 캐릭터 컨트롤러는 아닙니다. 매우 얇은 장애물은 빠른 이동에서 통과할 수 있습니다.
- Sphere/Capsule의 비균일 Scale은 보수적인 반경으로 처리합니다. 부모 비균일 Scale과 회전으로 생긴 Shear는 Box에 보수적 AABB를 사용합니다. 정확한 원형/OBB 표현이 필요하면 균일 Scale 계층을 사용하세요.
- SetParent는 Local TRS를 유지합니다. World Transform 유지 재부모화와 임의 Shear 편집은 제공하지 않습니다.
- UI 콜백과 Feature 객체는 Application 수명을 공유합니다. 동적 Feature 파괴에는 별도의 UI 등록 해제가 필요합니다.

## 자동 검증

```powershell
./Tools/Test.ps1 -Filter 'test_engine_*'
cmake -S . -B C:/temp/dx17 -G "Visual Studio 17 2022" -A x64
cmake --build C:/temp/dx17 --config Debug --parallel 1
ctest --test-dir C:/temp/dx17 -C Debug --output-on-failure
```

플레이 테스트는 사용자가 수행합니다. 구현·검증 기록은 Docs/IMPLEMENTATION.md와 Docs/VERIFICATION.md에 남깁니다.
