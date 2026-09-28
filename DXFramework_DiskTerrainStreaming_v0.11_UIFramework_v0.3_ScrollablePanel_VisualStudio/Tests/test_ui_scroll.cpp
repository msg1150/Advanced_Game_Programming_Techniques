// UIScroll의 범위/휠/Thumb Drag 계산은 DirectX 없이 검증할 수 있습니다.
#include "UI/UIScroll.h"
#include <cassert>
#include <cmath>
int main()
{
    assert(UIScroll::MaxOffset(600.f,300.f)==300.f);
    assert(UIScroll::MaxOffset(200.f,300.f)==0.f);
    assert(UIScroll::ClampOffset(999.f,600.f,300.f)==300.f);
    assert(UIScroll::ApplyWheel(100.f,1.f,600.f,300.f,40.f)==60.f);
    assert(UIScroll::ApplyWheel(100.f,-1.f,600.f,300.f,40.f)==140.f);

    const UIRect track{100.f,50.f,10.f,300.f};
    const auto top=UIScroll::BuildGeometry(track,900.f,300.f,0.f,30.f);
    assert(top.Scrollable);
    assert(std::fabs(top.Thumb.H-100.f)<0.001f);
    assert(std::fabs(top.Thumb.Y-50.f)<0.001f);

    const auto bottom=UIScroll::BuildGeometry(track,900.f,300.f,600.f,30.f);
    assert(std::fabs(bottom.Thumb.Y-250.f)<0.001f);
    assert(std::fabs(UIScroll::OffsetFromThumbTop(250.f,bottom)-600.f)<0.001f);
    assert(std::fabs(UIScroll::OffsetFromThumbTop(150.f,bottom)-300.f)<0.001f);
    return 0;
}
