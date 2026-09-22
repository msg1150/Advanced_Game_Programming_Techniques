// ============================================================================
// MiniMapZoomPolicy.h - 미니맵 화면 배율 전용 순수 계산 정책.
// CPU Tile 요청 / GPU 로딩 반경과 독립적이므로 Renderer 테스트 없이 검증 가능.
// ============================================================================
#pragma once
#include <algorithm>
#include <cmath>
namespace MiniMapZoomPolicy
{
    constexpr float MinimumPercent=50.f;
    constexpr float MaximumPercent=400.f;
    constexpr float DefaultPercent=100.f;

    inline float ClampPercent(float percent) noexcept
    {
        return std::isfinite(percent)
            ? std::clamp(percent,MinimumPercent,MaximumPercent)
            : DefaultPercent;
    }

    // World Unit 화면 반경: Zoom을 높이면 같은 고정 미니맵에 더 좁은 영역 표시.
    inline float ViewRadius(float unloadRadius,float percent) noexcept
    {
        const float baseRadius=std::max(85.f,unloadRadius*1.12f);
        return baseRadius*100.f/ClampPercent(percent);
    }
}
