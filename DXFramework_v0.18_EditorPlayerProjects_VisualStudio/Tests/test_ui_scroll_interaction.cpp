// 실제 UIManager를 Mock Renderer/Input과 링크해 Wheel Scroll과 Scrollbar Drag를 검증합니다.
#include "UI/UIManager.h"
#include "Input/Input.h"
#include <cassert>
#include <memory>

static void Step(UIManager& ui,Input& in,int x,int y,bool press,bool down,bool release,float wheel=0.f)
{
    in.x=x;in.y=y;in.pressed=press;in.down=down;in.released=release;in.wheel=wheel;
    ui.Update(in,1280.f,720.f);
    in.wheel=0.f;
}

static void Click(UIManager& ui,Input& in,int x,int y)
{
    Step(ui,in,x,y,true,true,false);
    Step(ui,in,x,y,false,false,true);
}

int main()
{
    UIManager ui;
    const auto p=ui.CreatePanel(L"Scroll Test",L"Scroll",L"Close",260.f,180.f);
    for(int i=0;i<12;++i)
        ui.AddToPanel(p,std::make_unique<UILabel>(L"row"));
    Input in;

    // 두 번째 Side Button을 열어 Scroll Panel 표시.
    Click(ui,in,1045,20);
    assert(ui.IsPanelOpen(p));

    // Panel은 x=508 부근, content y=105~226. Wheel Down은 UI가 소비해야 함.
    Step(ui,in,600,150,false,false,false,-1.f);
    assert(ui.IsWheelCaptured());

    // Panel 밖 Wheel은 Camera로 넘겨야 하므로 UI가 소비하지 않음.
    Step(ui,in,100,600,false,false,false,-1.f);
    assert(!ui.IsWheelCaptured());

    // Scrollbar Track 우측을 Drag. Mouse Capture가 유지되어야 함.
    Step(ui,in,748,150,true,true,false);
    assert(ui.IsMouseCaptured());
    Step(ui,in,748,215,false,true,false);
    assert(ui.IsMouseCaptured());
    Step(ui,in,748,215,false,false,true);
    assert(!ui.IsMouseCaptured());
    return 0;
}
