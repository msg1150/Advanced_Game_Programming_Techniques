// UIDemoPanel.h : Terrain/Streaming을 포함하지 않는 UI Framework 독립 검증 Feature.
#pragma once
class UIManager;
class UIDemoPanel
{
public:
    void Register(UIManager& ui);
private:
    bool checkbox_=false;
    float sliderValue_=50.f;
    unsigned int clicks_=0;
};
