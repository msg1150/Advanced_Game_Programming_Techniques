// ============================================================================
// UIManager.h : Widget 등록/화면 배치/Mouse Capture. Terrain 의존 없음.
// UI 조작 중 Orbit만 차단한다. WASD/휠 입력은 소비하지 않는다.
// ============================================================================
#pragma once
#include "UI/UIWidget.h"
#include <memory>
#include <string>
#include <vector>
class Input;
class UIManager
{
public:
    void Add(std::unique_ptr<UIWidget> widget);
    void Layout(float clientWidth,float clientHeight);
    // 반환값: Camera Orbit을 허용하는지. Mouse Wheel / WASD에는 영향 없음.
    bool Update(const Input& input,float clientWidth,float clientHeight);
    void Render(UIRenderer& renderer)const;
    bool IsMouseCaptured()const{return capturePanel_;}
private:
    UIPanel panel_;
    std::vector<std::unique_ptr<UIWidget>> widgets_;
    UIWidget* active_=nullptr;
    bool capturePanel_=false;
    float mouseX_=0.f,mouseY_=0.f;
};
