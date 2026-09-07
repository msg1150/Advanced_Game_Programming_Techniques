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

각 단계의 세부 구현 내용은 해당 프로젝트 폴더의 `README.md`에 정리합니다.
