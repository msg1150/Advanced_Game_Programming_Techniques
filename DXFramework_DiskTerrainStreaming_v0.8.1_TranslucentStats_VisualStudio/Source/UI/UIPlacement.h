// ============================================================================
// UIPlacement.h : 창 크기에 따른 UI 배치/이동 제한 (플랫폼 독립).
// 다른 UI 패널에서도 동일한 위치 제한 로직을 사용할 수 있다.
// ============================================================================
#pragma once
#include <algorithm>
namespace UIPlacement
{
    struct Rect
    {
        float X=0.f,Y=0.f,W=0.f,H=0.f;
        bool Contains(float x,float y)const noexcept
        {return x>=X && y>=Y && x<X+W && y<Y+H;}
    };

    inline Rect SideButton(float clientWidth)
    {
        const float width=std::min(112.f,std::max(0.f,clientWidth-16.f));
        return {std::max(0.f,clientWidth-width-12.f),12.f,width,34.f};
    }

    inline Rect ClampPanel(float desiredX,float desiredY,float panelWidth,
                           float panelHeight,float clientWidth,float clientHeight)
    {
        const float xMax=std::max(0.f,clientWidth-panelWidth-8.f);
        const float yMax=std::max(0.f,clientHeight-panelHeight-8.f);
        return {std::clamp(desiredX,0.f,xMax),
                std::clamp(desiredY,0.f,yMax),panelWidth,panelHeight};
    }
}
