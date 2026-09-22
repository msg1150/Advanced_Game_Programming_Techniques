# Disk Terrain Streaming v0.8.1 — 반투명 성능 통계 패널

## 변경 내용

Windows에서 실행 확인한 v0.8 접이식 한글 UI를 기준으로, 왼쪽 **「지형 성능 통계」 패널 배경만** 불투명도 `0.95`에서 `0.68`로 변경했습니다. 뒤의 지형이 약 32% 보이도록 배경을 블렌딩하면서 통계 글자와 테두리는 그대로 불투명하게 표시합니다. 오른쪽 「지형 설정」 창, 열기/닫기 버튼, 미니맵 등은 변경하지 않았습니다.

변경 위치: `Source/Features/TerrainUI/TerrainSettingsPanel.cpp`의 `TerrainSettingsPanel::RenderStats()`

```cpp
// 1.0f = 완전 불투명, 0.0f = 완전 투명
constexpr float kStatsBackgroundOpacity=0.68f;
renderer.FillRoundRect({x,y,panelWidth,570.f},
                       {.065f,.09f,.13f,kStatsBackgroundOpacity},8.f);
```

투명도 선호에 따라 `0.68f`를 `0.55f`(더 투명) 또는 `0.80f`(더 진함)로 조절할 수 있습니다. 글자/테두리의 불투명도는 변경하지 마세요.

## 설치 및 확인

1. 기존 v0.8 폴더는 보관하고 이 v0.8.1 폴더의 `DXFramework.sln`을 Visual Studio 2022에서 엽니다.
2. `Debug | x64` 빌드 및 실행한 뒤 설정창의 `디버그 통계` 체크박스를 켭니다.
3. 통계 숫자의 가독성은 유지하면서 좌측 패널 뒤의 Terrain이 보이는지 확인합니다. 설정창 이동/접기, 슬라이더, 프리페치/캐시/Streaming 동작도 그대로여야 합니다.

**변경 범위:** 성능 통계 UI 배경 색상의 알파값 및 단계별 README뿐. Terrain, 캐시, Worker, DirectX 초기화 및 나머지 소스는 변경하지 않았습니다. Windows MSVC/GPU 실제 빌드는 이 작업 환경에서 수행하지 못하므로 로컬 PC에서 확인이 필요합니다.

이전 v0.8 상세 설명은 `README_previous_v0.8.md`로 보존했습니다. 저장소 루트 README는 변경하지 않습니다.
