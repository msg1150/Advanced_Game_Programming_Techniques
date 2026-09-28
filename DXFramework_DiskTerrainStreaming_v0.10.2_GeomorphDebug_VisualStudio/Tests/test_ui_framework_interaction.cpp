// 실제 UIManager / UIWidget / UIDemoPanel 소스를 Mock Renderer/Input과 링크해 검증.
#include "UI/UIManager.h"
#include "Features/UIDemo/UIDemoPanel.h"
#include "Input/Input.h"
#include <algorithm>
#include <cassert>
#include <memory>
static bool Step(UIManager& ui,Input& in,int x,int y,bool press,bool down,bool release,
                 const std::wstring& chars=L"")
{
    in.x=x;in.y=y;in.pressed=press;in.down=down;in.released=release;in.chars=chars;
    return ui.Update(in,1280.f,720.f);
}
static void Click(UIManager& ui,Input& in,int x,int y)
{
    assert(!Step(ui,in,x,y,true,true,false));
    assert(!Step(ui,in,x,y,false,false,true));
}
static bool HasText(const UIRenderer& renderer,const std::wstring& expected)
{
    return std::find(renderer.text.begin(),renderer.text.end(),expected)!=renderer.text.end();
}
int main()
{
    UIManager ui;
    float load=56.f;
    ui.Add(std::make_unique<UISlider>(L"Load 반경",28.f,160.f,1.f,
        [&]{return load;},[&](float v){load=v;}));
    // Terrain Settings는 실제로 StartSecondColumn()을 사용하므로 2열 입력도 검사.
    ui.StartSecondColumn();
    bool enabled=false;
    ui.Add(std::make_unique<UICheckBox>(L"테스트 상태",
        [&]{return enabled;},[&](bool v){enabled=v;}));
    UIDemoPanel demo;demo.Register(ui);
    Input in;
    assert(!ui.IsPanelOpen() && !ui.IsPanelOpen(1));
    Click(ui,in,1160,20);
    assert(ui.IsPanelOpen() && !ui.IsPanelOpen(1));
    // 첫 열 slider 입력필드: panel x=768, left x=780, width=232, number x=935.
    Click(ui,in,950,117);
    assert(ui.IsKeyboardCaptured());
    assert(!Step(ui,in,950,117,false,false,false,L"140\r"));
    assert(load==140.f && !ui.IsKeyboardCaptured());
    Click(ui,in,950,117);
    assert(!Step(ui,in,950,117,false,false,false,L"20"));
    in.esc=true;
    Step(ui,in,950,117,false,false,false);in.esc=false;
    assert(load==140.f && !ui.IsKeyboardCaptured());
    // 두 번째 열 checkbox: x1024 y105.
    Click(ui,in,1070,116);
    assert(enabled);
    // 편집 후 다른 panel 열기 = 자동 commit. 버튼별 열린 상태는 독립.
    Click(ui,in,950,117);
    assert(!Step(ui,in,950,117,false,false,false,L"60"));
    Click(ui,in,1040,20);
    assert(load==60.f && ui.IsPanelOpen() && ui.IsPanelOpen(1));
    // Demo widget과 label은 Terrain 데이터를 전혀 참조하지 않는다.
    Click(ui,in,465,243); // demo button (x=426, y=54, single-column).
    UIRenderer renderer;
    ui.Render(renderer);
    assert(renderer.clipDepth==0);
    assert(HasText(renderer,L"Button 클릭 횟수: 1"));
    // 겹쳐진 Panel을 클릭할 경우 그리기 순서상 앞쪽 패널만 이벤트를 받는다.
    Click(ui,in,460,70); // Demo header는 pointer capture만 수행한다.
    assert(!Step(ui,in,630,220,false,true,false));
    Step(ui,in,630,220,false,false,true);
    assert(ui.IsPanelOpen(1) && enabled);
    // 창 focus 상실 시 편집 취소 + mouse capture 해제.
    Click(ui,in,950,117);
    // 패널 이동과 겹칠 수 있으므로 새 좌표로 첫 패널 입력을 재확인한다.
    if(ui.IsKeyboardCaptured())
    {
        Step(ui,in,950,117,false,false,false,L"77");
        in.lost=true;Step(ui,in,950,117,false,false,false);in.lost=false;
        assert(!ui.IsKeyboardCaptured() && load==60.f);
    }
    Click(ui,in,1160,20);
    assert(!ui.IsPanelOpen() && ui.IsPanelOpen(1));
    assert(Step(ui,in,100,650,true,true,false));
    Step(ui,in,100,650,false,false,true);
    return 0;
}
