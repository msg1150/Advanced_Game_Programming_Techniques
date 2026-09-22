// UIDemoPanel.cpp : 실제 게임 로직 없이 두 번째 Panel/Widget/Callback 재사용 확인.
#include "Features/UIDemo/UIDemoPanel.h"
#include "UI/UIManager.h"
#include <memory>
#include <string>
void UIDemoPanel::Register(UIManager& ui)
{
    const auto panel=ui.CreatePanel(L"UI Framework 테스트",L"UI 테스트",L"테스트 닫기",
                                    330.f,295.f);
    ui.AddToPanel(panel,std::make_unique<UILabel>(L"Terrain과 독립적인 Widget 테스트"));
    ui.AddToPanel(panel,std::make_unique<UICheckBox>(L"테스트 Checkbox",
        [this]{return checkbox_;},[this](bool checked){checkbox_=checked;}));
    ui.AddToPanel(panel,std::make_unique<UISlider>(L"테스트 Slider",
        0.f,100.f,5.f,[this]{return sliderValue_;},
        [this](float value){sliderValue_=value;}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"테스트 Button",
        [this]{++clicks_;}));
    ui.AddToPanel(panel,std::make_unique<UILabel>([this]
    {
        return std::wstring(L"Button 클릭 횟수: ")+std::to_wstring(clicks_);
    }));
    ui.AddToPanel(panel,std::make_unique<UILabel>(L"숫자칸 클릭: Enter 적용 / Esc 취소"));
}
