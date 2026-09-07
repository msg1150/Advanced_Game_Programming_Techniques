# 고급게임프로그래밍기법

고급게임프로그래밍기법 수업에서 진행한 실습 및 과제 작업물을 정리하는 저장소입니다.

## 목적

- 수업에서 구현한 기능과 예제를 주차별 또는 주제별로 관리
- 소스 코드와 필요한 프로젝트 파일을 Git으로 버전 관리
- 구현 과정과 결과를 기록하여 이후 복습 및 포트폴리오 참고 자료로 활용

## Repository 구성

프로젝트가 늘어나면 아래와 같이 주제 또는 주차별 폴더로 분리하여 관리합니다.

```text
.
├─ README.md
├─ .gitignore
├─ Week01/
├─ Week02/
├─ Week03/
└─ ...
```

필요한 경우 주차 대신 기능명이나 프로젝트명을 사용할 수 있습니다.

```text
.
├─ README.md
├─ .gitignore
├─ Rendering/
├─ AI/
├─ Network/
└─ FinalProject/
```

## 작업물 작성 방식

각 작업 폴더에는 가능하면 별도의 `README.md`를 두고 다음 내용을 기록합니다.

- 작업 주제
- 구현 목표
- 사용 기술
- 주요 구현 내용
- 실행 방법
- 결과 및 확인 사항

예시:

```md
# 작업명

## 목표
이번 작업에서 구현하려는 내용을 작성합니다.

## 주요 구현
- 기능 1
- 기능 2
- 기능 3

## 실행 방법
프로젝트 실행 및 테스트 방법을 작성합니다.

## 결과
구현 결과와 확인한 내용을 작성합니다.
```

## 개발 환경

수업 진행에 따라 실제 사용 환경을 아래에 추가합니다.

- Language:
- Engine / Framework:
- IDE:
- Platform:

## Git 관리 기준

저장소에는 직접 작성하거나 프로젝트 실행에 필요한 원본 파일을 중심으로 업로드합니다.

빌드 과정에서 자동 생성되는 파일, 캐시, 로그, IDE 개인 설정 파일 등은 `.gitignore`를 통해 제외합니다.

특히 Unreal Engine 프로젝트를 사용하는 경우 일반적으로 다음 폴더는 Git에 포함하지 않습니다.

- `Binaries/`
- `DerivedDataCache/`
- `Intermediate/`
- `Saved/`
- `.vs/`

반대로 다음 항목은 프로젝트 구성에 필요하므로 일반적으로 Git에 포함합니다.

- `Config/`
- `Content/`
- `Plugins/`
- `Source/`
- `*.uproject`
- `*.uplugin`

## Commit Message 예시

```text
Add: 새로운 기능 추가
Fix: 오류 수정
Update: 기존 기능 수정
Refactor: 코드 구조 개선
Docs: 문서 수정
```

예시:

```text
Add: Week01 vector practice
Fix: character movement issue
Update: README project description
```

## Notes

수업이 진행되면서 저장소 구조와 문서는 작업 내용에 맞게 계속 갱신합니다.
