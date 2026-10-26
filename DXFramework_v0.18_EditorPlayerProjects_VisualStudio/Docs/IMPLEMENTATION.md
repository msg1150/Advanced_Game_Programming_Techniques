# v0.18 구현 기록

- v0.17을 새 폴더로 복제했습니다. 기존 버전과 사용자 Git 변경은 보존했습니다.
- RuntimeSession으로 Scene 생성/업데이트/렌더링/충돌/접지/자원을 분리했습니다. EngineRuntimeFeature는 Editor UI 어댑터입니다.
- 공통 UTF-8 변환기를 Core/Text로 이동하고 기존 UIUtf8 이름은 별칭으로 호환합니다.
- Collision Debug/Geometry 구현을 Graphics/Debug로 이동하고 기존 헤더를 호환 래퍼로 유지했습니다.
- DXRuntime.lib를 Editor/Player가 함께 링크합니다. Player 소스는 Editor/UI/Tool 헤더를 포함하지 않습니다.
- ProjectFile/ProjectManager는 .dxproject와 템플릿 데이터 복사만 담당합니다. 파일 열기는 검증 후 출력 객체를 교체하며, 기존 디렉터리에 프로젝트를 덮어쓰지 않습니다.
- 프로젝트 생성은 별도 .creating.* 폴더에서 준비한 후 최종 이름으로 이동합니다. 실패 시 사용자 데이터를 재귀 삭제하지 않습니다.
- 경로 변환은 Paths의 UTF-8 ↔ native path 경계로 모았습니다. Windows 경로 비교는 대소문자 차이를 고려합니다.
- Win32 파일 대화상자에 초기 파일명 옵션을 추가하고 기존 호출 기본값은 유지했습니다.
- 원자적 파일 교체를 Core/IO/AtomicFile로 모아 Scene/Project/최근 목록에서 재사용합니다.
- Native UnicodeProcess는 CreateProcessW와 별도 ApplicationName을 사용하며, 문자열을 쉘 명령으로 실행하지 않습니다.
- Player 실행은 저장본 기반이며, 종료는 WM_CLOSE 요청입니다. DLL/IPC와 강제 프로세스 종료를 도입하지 않았습니다.
- 한글 Shader 경로와 include 처리를 위해 직접 UTF-8 소스를 읽고 include 콜백에서 filesystem 경로로 파일을 엽니다.
- Source/Assets 데이터는 기존 형식을 재사용했습니다. 샘플 Scene을 C++ 코드에 다시 정의하지 않았습니다.
- Native 솔루션, CMake, Unicode-aware Build 스크립트와 핵심 회귀 검사를 추가했습니다.

이번 작업 범위는 계획의 v0.18 기반 단계입니다. 본격적인 Hierarchy/Inspector 작업 화면과 Edit/Play 복제 구조는 별도 후속 버전에서 진행합니다.
