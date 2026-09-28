// 실제 UIManager/Widget + UIDocument Loader를 Portable Mock Renderer/Input과 링크합니다.
#include "UI/Document/UIDocumentLoader.h"
#include "UI/Document/UIBindingRegistry.h"
#include "UI/UIManager.h"
#include <cassert>
int main()
{
    bool enabled=false;float value=25.f;int clicks=0;
    UIBindingRegistry bindings;
    bindings.RegisterBool("Demo.Enabled",[&]{return enabled;},[&](bool v){enabled=v;});
    bindings.RegisterFloat("Demo.Value",[&]{return value;},[&](float v){value=v;});
    bindings.RegisterText("Demo.Text",[&]{return enabled?L"ON":L"OFF";});
    bindings.RegisterAction("Demo.Click",[&]{++clicks;});
    const char* xml=R"XML(<UIDocument version="1"><Panel id="P" title="P" width="300" height="300"><Column>
      <CheckBox id="C" text="C" binding="Demo.Enabled" />
      <Slider id="S" text="S" min="0" max="100" step="1" binding="Demo.Value" />
      <Button id="B" text="B" action="Demo.Click" />
      <Label id="L" textBinding="Demo.Text" />
    </Column></Panel></UIDocument>)XML";
    UIManager ui;auto result=UIDocumentLoader::LoadString(ui,bindings,xml);
    assert(result.Success&&result.Instance.Panels.count("P")==1u);
    assert(result.Instance.Widgets.size()==4u);
    auto* check=result.Instance.Widgets.at("C");check->SetBounds({0,0,100,30});check->PointerUp(10,10);assert(enabled);
    auto* button=result.Instance.Widgets.at("B");button->SetBounds({0,0,100,30});button->PointerUp(10,10);assert(clicks==1);
    return 0;
}
