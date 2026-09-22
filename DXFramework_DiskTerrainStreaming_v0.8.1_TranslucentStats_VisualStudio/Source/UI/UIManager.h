// ============================================================================
// UIManager.h : 범용 위젯 등록, 접이식 패널, 제목줄 Drag, 입력 점유.
// Terrain에는 의존하지 않는다. UI 값 적용은 개별 Widget Callback 담당.
// ============================================================================
#pragma once
#include "UI/UIWidget.h"
#include <cstddef>
#include <memory>
#include <vector>
class Input;
class UIManager
{
public:
    UIManager();
    void Add(std::unique_ptr<UIWidget> widget);
    void StartSecondColumn(){secondColumnIndex_=widgets_.size();}
    void Layout(float clientWidth,float clientHeight);
    // 반환값: 좌클릭 Orbit 허용 여부. WASD/휠에는 직접 관여하지 않는다.
    bool Update(const Input& input,float clientWidth,float clientHeight);
    void Render(UIRenderer& renderer)const;
    bool IsMouseCaptured()const{return capturePanel_;}
    bool IsKeyboardCaptured()const{return focused_!=nullptr;}
    bool IsPanelOpen()const{return panelOpen_;}

private:
    void TogglePanel();
    void CommitFocusedEdit();

    UIPanel panel_;
    UIButton toggleButton_;
    std::vector<std::unique_ptr<UIWidget>> widgets_;
    std::size_t secondColumnIndex_=static_cast<std::size_t>(-1);
    UIWidget* active_=nullptr;
    UIWidget* focused_=nullptr;

    // 접힌 상태에서는 우측 상단 버튼만 남겨 Terrain 화면을 가리지 않는다.
    bool panelOpen_=false;
    bool manualPosition_=false;
    bool draggingHeader_=false;
    bool suppressOrbitUntilRelease_=false;
    bool capturePanel_=false;
    float panelX_=0.f,panelY_=54.f;
    float grabOffsetX_=0.f,grabOffsetY_=0.f;
    float mouseX_=0.f,mouseY_=0.f;
    float viewportWidth_=1280.f,viewportHeight_=720.f;
};
