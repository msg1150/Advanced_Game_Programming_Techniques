#include "UI/UIPlacement.h"
#include <cassert>
#include <cmath>
int main()
{
    using namespace UIPlacement;
    auto tab=SideButton(1280.f);
    assert(tab.X==1156.f && tab.Y==12.f && tab.W==112.f && tab.H==34.f);
    assert(tab.Contains(1160.f,15.f));
    assert(!tab.Contains(1100.f,15.f));
    tab=SideButton(80.f);
    assert(tab.W==64.f && tab.X==4.f);
    auto p=ClampPanel(768.f,54.f,500.f,425.f,1280.f,720.f);
    assert(p.X==768.f && p.Y==54.f);
    p=ClampPanel(-40.f,-10.f,500.f,425.f,1280.f,720.f);
    assert(p.X==0.f && p.Y==0.f);
    p=ClampPanel(10000.f,10000.f,500.f,425.f,1280.f,720.f);
    assert(p.X==772.f && p.Y==287.f);
    p=ClampPanel(200.f,100.f,500.f,425.f,480.f,420.f);
    assert(p.X==0.f && p.Y==0.f);
    return 0;
}
