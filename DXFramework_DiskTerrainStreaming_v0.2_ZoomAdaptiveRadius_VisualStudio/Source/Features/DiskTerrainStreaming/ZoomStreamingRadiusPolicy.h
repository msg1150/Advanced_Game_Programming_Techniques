// ============================================================================
// ZoomStreamingRadiusPolicy.h
// ----------------------------------------------------------------------------
// 카메라 Zoom Distance로 Load/Unload 반경을 결정하는 순수 계산 정책.
//
// - Win32 / Direct3D / Terrain Manager에 의존하지 않는다.
// - 기존 Tile 판정, Worker, QuadTree, Material은 수정하지 않는다.
// - 반경은 반드시 Min~Max 범위에서 유지된다.
// - UnloadRadius는 LoadRadius보다 커서 Hysteresis를 유지한다.
// ============================================================================
#pragma once

#include <algorithm>
#include <cmath>

namespace ZoomStreamingRadiusPolicy
{
    struct Settings
    {
        // OFF일 때는 Manager의 기존 고정 Load/Unload Radius를 그대로 사용한다.
        bool Enabled = true;

        // 기존 초기 Camera Distance=46에서 LoadRadius=56을 유지하기 위한 기준.
        float ReferenceCameraDistance = 46.0f;

        // Zoom In 시 Load Radius가 더 이상 줄지 않는 최소값.
        float MinimumLoadRadius = 28.0f;

        // Zoom Out 시 Load Radius가 더 이상 커지지 않는 최댓값.
        float MaximumLoadRadius = 160.0f;

        // Camera Distance 1 증가당 Load Radius 증가량.
        float RadiusPerDistance = 0.75f;

        // 이미 로드됐거나 로딩 중인 Tile이 더 멀리까지 유지되는 여유 거리.
        float UnloadMargin = 28.0f;

        // Unload Radius도 무한히 증가하지 않도록 별도 상한을 보관한다.
        float MaximumUnloadRadius = 188.0f;
    };

    struct Radii
    {
        float Load = 0.0f;
        float Unload = 0.0f;
    };

    inline bool IsValid(const Settings& value) noexcept
    {
        return std::isfinite(value.ReferenceCameraDistance) &&
               std::isfinite(value.MinimumLoadRadius) &&
               std::isfinite(value.MaximumLoadRadius) &&
               std::isfinite(value.RadiusPerDistance) &&
               std::isfinite(value.UnloadMargin) &&
               std::isfinite(value.MaximumUnloadRadius) &&
               value.ReferenceCameraDistance > 0.0f &&
               value.MinimumLoadRadius > 0.0f &&
               value.MaximumLoadRadius >= value.MinimumLoadRadius &&
               value.RadiusPerDistance >= 0.0f &&
               value.UnloadMargin > 0.0f &&
               // 어떤 Zoom 위치에서든 Unload > Load를 보장한다.
               value.MaximumUnloadRadius > value.MaximumLoadRadius &&
               value.MaximumUnloadRadius >=
                   value.MinimumLoadRadius + value.UnloadMargin;
    }

    inline Radii Calculate(float cameraDistance,
                           float referenceLoadRadius,
                           float referenceUnloadRadius,
                           const Settings& settings) noexcept
    {
        // Zoom 연동이 OFF이면 기존 정책을 일절 변경하지 않는다.
        if (!settings.Enabled)
        {
            return {referenceLoadRadius, referenceUnloadRadius};
        }

        if (!IsValid(settings))
        {
            // 잘못된 설정에서 판정 범위를 망가뜨리지 않도록 고정값으로 복귀.
            return {referenceLoadRadius, referenceUnloadRadius};
        }

        // 외부 Camera 값이 유효하지 않더라도 최소한 기준 Zoom으로 처리한다.
        const float safeDistance = std::isfinite(cameraDistance)
            ? std::max(cameraDistance, 0.0f)
            : settings.ReferenceCameraDistance;

        const float requestedLoad =
            referenceLoadRadius +
            (safeDistance - settings.ReferenceCameraDistance) *
                settings.RadiusPerDistance;

        const float load = std::clamp(requestedLoad,
                                      settings.MinimumLoadRadius,
                                      settings.MaximumLoadRadius);

        const float unload = std::min(load + settings.UnloadMargin,
                                      settings.MaximumUnloadRadius);
        return {load, unload};
    }
}
