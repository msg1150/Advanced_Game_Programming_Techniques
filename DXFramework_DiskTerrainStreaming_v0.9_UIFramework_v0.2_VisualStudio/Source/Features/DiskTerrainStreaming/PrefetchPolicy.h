// ============================================================================
// PrefetchPolicy.h — Camera Target 이동 속도로부터 선행 위치의 XZ 오프셋 계산.
// Win32/Direct3D 비의존. Zoom, 카메라 회전, GPU 렌더링과 완전히 독립.
// ============================================================================
#pragma once
#include <algorithm>
#include <cmath>
namespace PrefetchPolicy
{
    struct Offset {float X=0.f,Z=0.f;};
    inline Offset PredictOffset(float velocityX,float velocityZ,
                                float lookaheadSeconds,float maximumLead) noexcept
    {
        if(!std::isfinite(velocityX) || !std::isfinite(velocityZ) ||
           !std::isfinite(lookaheadSeconds) || !std::isfinite(maximumLead) ||
           lookaheadSeconds<=0.f || maximumLead<=0.f)return {};
        const float speed=std::sqrt(velocityX*velocityX+velocityZ*velocityZ);
        if(speed<=.25f || !std::isfinite(speed))return {};
        const float lead=std::min(maximumLead,speed*lookaheadSeconds);
        return {velocityX*(lead/speed),velocityZ*(lead/speed)};
    }
}
