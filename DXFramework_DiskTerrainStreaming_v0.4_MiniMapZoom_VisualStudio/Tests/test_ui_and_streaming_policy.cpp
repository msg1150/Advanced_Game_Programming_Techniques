// Linux/Windows 단독 테스트: 렌더러 없이 UI 슬라이더/Streaming 판정 정책 확인.
#include "Features/DiskTerrainStreaming/StreamingRadiusPolicy.h"
#include "Features/DiskTerrainStreaming/MiniMapZoomPolicy.h"
#include "UI/UISliderMath.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <iostream>
int main()
{
    const StreamingRadiusPolicy::Settings s={28.f,160.f,28.f,188.f};
    assert(StreamingRadiusPolicy::IsValid(s));
    const auto initial=StreamingRadiusPolicy::Calculate(56.f,s);
    assert(initial.Load==56.f && initial.Unload==84.f);
    const auto minimum=StreamingRadiusPolicy::Calculate(-999.f,s);
    assert(minimum.Load==28.f && minimum.Unload==56.f);
    const auto maximum=StreamingRadiusPolicy::Calculate(999.f,s);
    assert(maximum.Load==160.f && maximum.Unload==188.f);
    const auto invalid=StreamingRadiusPolicy::Calculate(
        std::numeric_limits<float>::quiet_NaN(),s);
    assert(invalid.Load==28.f && invalid.Unload==56.f);
    assert(UISliderMath::ValueFromX(0.f,0.f,100.f,28.f,160.f,1.f)==28.f);
    assert(UISliderMath::ValueFromX(50.f,0.f,100.f,28.f,160.f,1.f)==94.f);
    assert(UISliderMath::ValueFromX(1000.f,0.f,100.f,28.f,160.f,1.f)==160.f);
    assert(UISliderMath::Fraction(94.f,28.f,160.f)==.5f);
    // 지도만 확대: 지도 패널 크기, Camera Zoom 및 Tile 로딩 정책 독립.
    using namespace MiniMapZoomPolicy;
    assert(ClampPercent(-99.f)==MinimumPercent);
    assert(ClampPercent(999.f)==MaximumPercent);
    assert(ClampPercent(std::numeric_limits<float>::quiet_NaN())==DefaultPercent);
    assert(std::abs(ViewRadius(84.f,100.f)-94.08f)<0.001f);
    assert(std::abs(ViewRadius(84.f,200.f)-47.04f)<0.001f);
    assert(std::abs(ViewRadius(84.f,50.f)-188.16f)<0.001f);
    assert(ViewRadius(84.f,400.f)<ViewRadius(84.f,100.f));
    assert(ViewRadius(84.f,100.f)<ViewRadius(84.f,50.f));
    assert(UISliderMath::ValueFromX(0.f,0.f,100.f,50.f,400.f,25.f)==50.f);
    assert(UISliderMath::ValueFromX(100.f,0.f,100.f,50.f,400.f,25.f)==400.f);
    // Camera Distance는 Calculate() 파라미터에 존재하지 않는다.
    std::cout << "PASS: independent mini-map grid zoom + streaming radius + slider bounds" << std::endl;
}
