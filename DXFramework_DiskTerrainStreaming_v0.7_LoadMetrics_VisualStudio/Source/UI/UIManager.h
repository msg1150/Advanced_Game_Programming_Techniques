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
    // 기존 Settings와 Prefetch/Cache 설정을 별도 열로 분리한다.
    void StartSecondColumn(){secondColumnIndex_=widgets_.size();}
    void Layout(float clientWidth,float clientHeight);
    // 반환값: Camera Orbit을 허용하는지. Mouse Wheel / WASD에는 영향 없음.
    bool Update(const Input& input,float clientWidth,float clientHeight);
    void Render(UIRenderer& renderer)const;
    bool IsMouseCaptured()const{return capturePanel_;}
    // 이 값이 참일 때 WASD를 카메라에 전달하지 않는다.
    bool IsKeyboardCaptured()const{return focused_!=nullptr;}

private:
    UIPanel panel_;
    std::vector<std::unique_ptr<UIWidget>> widgets_;
    std::size_t secondColumnIndex_=static_cast<std::size_t>(-1);
    UIWidget* active_=nullptr;
    UIWidget* focused_=nullptr;
    // 텍스트 입력칸 밖 클릭 시 같은 클릭으로 Orbit이 시작되지 않게 방어.
    bool suppressOrbitUntilRelease_=false;
    bool capturePanel_=false;
    float mouseX_=0.f,mouseY_=0.f;
};
