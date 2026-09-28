// ============================================================================
// UIDemoPanel.cpp
// ----------------------------------------------------------------------------
// v0.4부터 Panel/Widget 구조는 Assets/UI/UIDemo.ui.xml에서 읽고,
// 이 C++ 파일은 실제 값/Action Binding만 제공합니다.
// ============================================================================
#include "Features/UIDemo/UIDemoPanel.h"
#include "UI/Document/UIDocumentLoader.h"
#include "UI/UIManager.h"
#include <memory>
#include <string>
void UIDemoPanel::RegisterBindings()
{
    bindings_.Clear();
    bindings_.RegisterBool("Demo.Enabled",[this]{return checkbox_;},[this](bool value){checkbox_=value;});
    bindings_.RegisterFloat("Demo.Value",[this]{return sliderValue_;},[this](float value){sliderValue_=value;});
    bindings_.RegisterText("Demo.ClickText",[this]{return ClickText();});
    bindings_.RegisterAction("Demo.Click",[this]{++clicks_;});
    bindings_.RegisterAction("Demo.Reset",[this]{checkbox_=false;sliderValue_=50.f;clicks_=0u;});
}
std::wstring UIDemoPanel::ClickText()const
{return std::wstring(L"Button 클릭 횟수: ")+std::to_wstring(clicks_);}
void UIDemoPanel::Register(UIManager& ui,const std::filesystem::path& documentPath)
{
    RegisterBindings();
    auto result=UIDocumentLoader::LoadFile(ui,bindings_,documentPath);
    diagnostics_=std::move(result.Diagnostics);
    if(result.Success)
    {
        documentInstance_=std::move(result.Instance);
        return;
    }
    // UI Asset이 깨져도 Terrain 실행 자체가 실패하면 안 됩니다.
    // 기존 Code-driven API도 계속 지원하므로 최소 Demo를 Fallback으로 생성합니다.
    RegisterFallback(ui);
}
void UIDemoPanel::RegisterFallback(UIManager& ui)
{
    const auto panel=ui.CreatePanel(L"UI Document 오류 / Fallback",L"UI 테스트",L"테스트 닫기",350.f,320.f);
    ui.AddToPanel(panel,std::make_unique<UILabel>(L"UIDemo.ui.xml을 읽지 못해 Code UI를 사용합니다."));
    ui.AddToPanel(panel,std::make_unique<UICheckBox>(L"테스트 Checkbox",
        [this]{return checkbox_;},[this](bool checked){checkbox_=checked;}));
    ui.AddToPanel(panel,std::make_unique<UISlider>(L"테스트 Slider",0.f,100.f,5.f,
        [this]{return sliderValue_;},[this](float value){sliderValue_=value;}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"테스트 Button",[this]{++clicks_;}));
    ui.AddToPanel(panel,std::make_unique<UILabel>([this]{return ClickText();}));
}
