// UI style/layout and side-tab placement: DirectX 없는 순수 배치 테스트.
#include "UI/UILayout.h"
#include "UI/UIPlacement.h"
#include <cassert>
#include <cmath>
#include <vector>
int main()
{
    using namespace UIPlacement;
    const auto mainButton=SideButtonIndexed(1280.f,0u,2u);
    const auto demoButton=SideButtonIndexed(1280.f,1u,2u);
    assert(mainButton.X==1156.f && mainButton.W==112.f);
    assert(demoButton.X==1036.f && demoButton.X+demoButton.W<mainButton.X);
    const auto a=SideButtonIndexed(80.f,0u,2u);
    const auto b=SideButtonIndexed(80.f,1u,2u);
    assert(a.W>0.f && b.W>0.f && b.X+b.W<a.X);
    const UIRect panel{768.f,54.f,500.f,425.f};
    const auto layout=UILayout::Arrange(panel,{22.f,28.f,66.f,22.f,28.f},3u);
    assert(layout.Items.size()==5u);
    assert(layout.Items[0].X==780.f && layout.Items[0].Y==105.f);
    assert(layout.Items[1].Y==129.f && layout.Items[2].Y==159.f);
    assert(layout.Items[3].X==1024.f && layout.Items[3].Y==105.f);
    assert(layout.Items[4].Y==129.f);
    assert(layout.ContentClip.Contains(layout.Items[4].X+2.f,layout.Items[4].Y+2.f));
    assert(!layout.ContentClip.Contains(panel.X,layout.Items[0].Y));
    const auto single=UILayout::Arrange({426.f,54.f,330.f,295.f},
                                         {22.f,28.f,66.f,34.f},static_cast<std::size_t>(-1));
    assert(single.Items[0].W==306.f && single.Items[3].Y==227.f);
    const auto tiny=UILayout::Arrange({0.f,0.f,100.f,58.f},{66.f,22.f},1u);
    assert(tiny.ContentClip.H==0.f); // 작아지면 위젯은 잘리고 Panel 밖 클릭을 받지 않음.
    assert(!UILayout::Intersects({0.f,80.f,20.f,20.f},tiny.ContentClip));
    return 0;
}
