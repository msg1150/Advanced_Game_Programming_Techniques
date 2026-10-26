// ============================================================================
// StreamingRadiusPolicy.h : Zoom과 독립적인 UI 반경 범위 및 Hysteresis 정책.
// Direct3D/Win32 의존성이 없는 순수 함수로 단독 테스트할 수 있다.
// ============================================================================
#pragma once
#include <algorithm>
#include <cmath>
namespace StreamingRadiusPolicy
{
    struct Settings
    {
        float MinimumLoad = 28.0f;
        float MaximumLoad = 160.0f;
        float UnloadMargin = 28.0f;
        float MaximumUnload = 188.0f;
    };
    struct Radii {float Load = 56.0f; float Unload = 84.0f;};
    inline bool IsValid(const Settings& s) noexcept
    {
        return std::isfinite(s.MinimumLoad) && std::isfinite(s.MaximumLoad) &&
               std::isfinite(s.UnloadMargin) && std::isfinite(s.MaximumUnload) &&
               s.MinimumLoad > 0.0f && s.MaximumLoad >= s.MinimumLoad &&
               s.UnloadMargin > 0.0f &&
               s.MaximumUnload >= s.MaximumLoad + s.UnloadMargin;
    }
    inline Radii Calculate(float requestedLoad, const Settings& s) noexcept
    {
        if(!IsValid(s))return {};
        const float safe = std::isfinite(requestedLoad) ? requestedLoad : s.MinimumLoad;
        const float load = std::clamp(safe,s.MinimumLoad,s.MaximumLoad);
        return {load,std::min(load + s.UnloadMargin,s.MaximumUnload)};
    }
}
