// ============================================================================
// UISliderMath.h : Slider의 마우스 위치 <-> 값 환산 (플랫폼 독립).
// ============================================================================
#pragma once
#include <algorithm>
#include <cmath>
namespace UISliderMath
{
    inline float ValueFromX(float x,float left,float width,
                            float minimum,float maximum,float step) noexcept
    {
        if(width<=0.f || maximum<=minimum || step<=0.f)return minimum;
        const float t=std::clamp((x-left)/width,0.f,1.f);
        const float raw=minimum+t*(maximum-minimum);
        const float quantized=minimum+std::round((raw-minimum)/step)*step;
        return std::clamp(quantized,minimum,maximum);
    }
    inline float Fraction(float value,float minimum,float maximum) noexcept
    {
        if(maximum<=minimum)return 0.f;
        return std::clamp((value-minimum)/(maximum-minimum),0.f,1.f);
    }
}
