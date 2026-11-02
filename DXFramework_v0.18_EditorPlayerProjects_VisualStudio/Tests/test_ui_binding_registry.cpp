#include "UI/Document/Model/UIBindingRegistry.h"
#include <cassert>
int main()
{
    UIBindingRegistry registry;bool b=false;float f=1.f;int calls=0;
    assert(registry.RegisterBool("B",[&]{return b;},[&](bool v){b=v;}));
    assert(registry.RegisterFloat("F",[&]{return f;},[&](float v){f=v;}));
    assert(registry.RegisterText("T",[]{return std::wstring(L"text");}));
    assert(registry.RegisterAction("A",[&]{++calls;}));
    assert(!registry.RegisterAction("A",[]{}));
    const auto* bb=registry.FindBool("B");assert(bb&&bb->Get()==false);bb->Set(true);assert(b);
    const auto* ff=registry.FindFloat("F");assert(ff);ff->Set(42.f);assert(f==42.f);
    assert(registry.FindText("T")&&(*registry.FindText("T"))()==L"text");
    assert(registry.FindAction("A"));(*registry.FindAction("A"))();assert(calls==1);
    assert(!registry.FindBool("missing"));
    return 0;
}
