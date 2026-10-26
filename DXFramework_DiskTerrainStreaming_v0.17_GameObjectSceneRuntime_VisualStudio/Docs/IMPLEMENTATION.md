# v0.17 구현 기록

- 베이스: main c6f33aa / v0.16. 새 v0.17 폴더만 수정했습니다.
- 순수 Scene/Component/Transform → Registry 기반 문서 → 자원/렌더 어댑터 → Collider 동기화/이벤트 → 고정 Step 이동 → Engine UI 순서로 연결했습니다.
- 기본 프레임워크에 새 GameObject 필드를 강제로 넣지 않고 Application에서 선택 Feature를 조립합니다.
- 기존 Mesh, Shader, Texture2D, CollisionWorld, CollisionDetector, CollisionMath, TerrainQuery, UIManager, UIWidget, UTF-8 변환, 안전한 파일 교체를 재사용합니다.
- 플랫폼 파일 대화상자에는 선택적 Options만 추가했으며 기존 호출의 기본 동작은 동일합니다.
- Scene과 Runtime 객체 핸들은 Scene Token + ObjectId입니다. 같은 문서를 재로드해도 이전 핸들은 새 객체를 참조하지 않습니다. Scene 수명 중 삭제된 ID도 재사용하지 않습니다.
- 수명 콜백 중 삭제/Component 제거는 표시 후 안전한 경계에서 처리합니다. 스냅샷 순회로 벡터 재할당을 피하고 Scene 교체도 요청 후 Update 경계에서 수행합니다.
- FixedDelta 기본 1/60초, 최대 Frame Delta .25초, 최대 Substep 8개로 무한 Catch-up을 제한합니다. SceneTiming으로 변경할 수 있습니다.
- 문서는 DXENGINE_SCENE version 1입니다. 최대 10,000 Objects, 객체당 128 Components, Component당 256 Properties, 문자열 65,536 bytes, 계층 깊이 128을 제한합니다. UI 파일 입력은 16 MiB로 제한합니다.
- 부모, 객체 참조, 중복 ID/Component/Property, 수치, Scale, UTF-8, 버전, 끝 데이터를 검증하고 후보 Scene이 준비된 뒤 교체합니다.
- Module이 없으면 MissingComponent로 원본 속성을 보존합니다. 알려진 Component는 References/RemapReferences 계약으로 참조를 확장합니다.
- 샘플 구성은 Assets/Scenes/Runtime.engine.scene이며 Mesh Asset은 Assets/Meshes/Cube.mesh입니다. 런타임 코드에 샘플 좌표나 장애물 목록을 고정하지 않았습니다.
- 기존 v0.16 Collider 편집 World와 새 Engine World는 독립입니다. 같은 Collision 알고리즘과 그래픽 클래스를 재사용하며 수명·저장 형식은 섞지 않습니다.

추가 범위가 아닌 항목: Rigidbody Solver, CCD, 애니메이션, FBX/glTF, 오디오, 네트워크, Undo/Redo, Engine Object용 GPU Picking, 런타임 DLL 언로드.
