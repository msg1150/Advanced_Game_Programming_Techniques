// ============================================================================
// test_zoom_streaming_radius_policy.cpp
// ----------------------------------------------------------------------------
// 단독 컴파일: g++ -std=c++20 -Wall -Wextra -pedantic -ISource
// Tests/test_zoom_streaming_radius_policy.cpp -o zoom_policy_test
// ============================================================================
#include "Features/DiskTerrainStreaming/ZoomStreamingRadiusPolicy.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

namespace Policy=ZoomStreamingRadiusPolicy;

bool Near(float a,float b){return std::abs(a-b)<0.001f;}

int main()
{
    const Policy::Settings cfg;
    assert(Policy::IsValid(cfg));

    // 과거 기본 Zoom에서 기존 Streaming 반경과 정확히 일치.
    const Policy::Radii base=Policy::Calculate(46.f,56.f,84.f,cfg);
    assert(Near(base.Load,56.f)&&Near(base.Unload,84.f));

    // Camera 최소 Distance에 가까워지면 더 작은 구역만 로딩한다.
    const Policy::Radii close=Policy::Calculate(2.f,56.f,84.f,cfg);
    assert(Near(close.Load,28.f)&&Near(close.Unload,56.f));

    // Camera 최대 Distance에 가까워져도 설정 최대값을 넘지 않는다.
    const Policy::Radii far=Policy::Calculate(200.f,56.f,84.f,cfg);
    assert(Near(far.Load,160.f)&&Near(far.Unload,188.f));
    const Policy::Radii extreme=Policy::Calculate(99999.f,56.f,84.f,cfg);
    assert(Near(extreme.Load,160.f)&&Near(extreme.Unload,188.f));

    float previousLoad=0.f;
    float previousUnload=0.f;
    for(int d=2;d<=200;++d)
    {
        const auto r=Policy::Calculate(static_cast<float>(d),56.f,84.f,cfg);
        assert(r.Load>=cfg.MinimumLoadRadius && r.Load<=cfg.MaximumLoadRadius);
        assert(r.Unload>r.Load && r.Unload<=cfg.MaximumUnloadRadius);
        assert(r.Load>=previousLoad && r.Unload>=previousUnload);
        previousLoad=r.Load;previousUnload=r.Unload;
    }

    // 옵션 비활성화 시 새로운 계산은 기존 반경을 덮어쓰지 않는다.
    Policy::Settings disabled=cfg;
    disabled.Enabled=false;
    const auto fixed=Policy::Calculate(200.f,56.f,84.f,disabled);
    assert(Near(fixed.Load,56.f)&&Near(fixed.Unload,84.f));

    // 비정상 Camera Distance는 기준 Camera Distance로 폴백.
    const auto invalid=Policy::Calculate(std::numeric_limits<float>::quiet_NaN(),56.f,84.f,cfg);
    assert(Near(invalid.Load,56.f)&&Near(invalid.Unload,84.f));

    // 유효성 실패를 조용히 무시하지 않는다.
    Policy::Settings bad=cfg;
    bad.MaximumUnloadRadius=150.f;
    assert(!Policy::IsValid(bad));

    std::cout << "PASS: base 56/84, near 28/56, far 160/188, "
              << "clamp, monotonic, hysteresis, disable, fallback, validation\n";
    return 0;
}
