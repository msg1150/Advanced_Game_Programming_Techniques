// UILayout.h : Widget 타입/게임 기능을 모르는 공통 세로/두 열 배치 계산.
// 숫자 입력, HitTest, 그리기는 UIManager 및 각 Widget이 담당한다.
#pragma once
#include "UI/UIRenderer.h"
#include "UI/UITheme.h"
#include <algorithm>
#include <cstddef>
#include <vector>
namespace UILayout
{
    struct Result
    {
        std::vector<UIRect> Items;
        UIRect ContentClip{};
        float RequiredContentHeight=0.f;
    };
    inline Result Arrange(const UIRect& panel,const std::vector<float>& heights,
                          std::size_t rightColumnStart)
    {
        Result result;
        const float left=panel.X+UITheme::PanelMargin;
        const float top=panel.Y+UITheme::PanelContentTop;
        const float innerWidth=std::max(0.f,panel.W-UITheme::PanelMargin*2.f);
        const bool twoColumns=rightColumnStart<heights.size();
        const float columnWidth=twoColumns?
            std::max(0.f,(innerWidth-UITheme::ColumnGap)/2.f):innerWidth;
        float leftY=top,rightY=top;
        result.Items.reserve(heights.size());
        for(std::size_t i=0;i<heights.size();++i)
        {
            const bool right=twoColumns && i>=rightColumnStart;
            float& y=right?rightY:leftY;
            const float height=std::max(0.f,heights[i]);
            result.Items.push_back({left+(right?columnWidth+UITheme::ColumnGap:0.f),
                                    y,columnWidth,height});
            y+=height+UITheme::ItemSpacing;
        }
        result.RequiredContentHeight=std::max(leftY,rightY)-top;
        // 패널 내부에서만 Widget을 렌더링/HitTest. 창이 작을 때 넘침 방지.
        result.ContentClip={panel.X+UITheme::PanelMargin,top,
                            innerWidth,std::max(0.f,panel.H-UITheme::PanelContentTop-8.f)};
        return result;
    }
    inline bool Intersects(const UIRect& a,const UIRect& b)
    {
        return a.W>0.f && a.H>0.f && b.W>0.f && b.H>0.f &&
               a.X<b.X+b.W && b.X<a.X+a.W && a.Y<b.Y+b.H && b.Y<a.Y+a.H;
    }
}
