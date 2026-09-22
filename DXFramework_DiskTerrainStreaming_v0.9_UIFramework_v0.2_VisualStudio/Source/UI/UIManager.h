// UIManager.h : Terrain을 모르는 범용 다중 패널 + Widget + 입력 라우터.
// 실제 게임 값은 Widget Callback만 알고, 다른 Feature도 CreatePanel/AddToPanel로 등록한다.
#pragma once
#include "UI/UIWidget.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
class Input;
class UIManager
{
public:
    using PanelId=std::size_t;
    UIManager();
    // 기본 Panel(0) 호환 API: 이전 TerrainSettingsPanel 등록 코드를 유지한다.
    void Add(std::unique_ptr<UIWidget> widget){AddToPanel(0,std::move(widget));}
    void StartSecondColumn(){StartSecondColumn(0);}
    bool IsPanelOpen()const{return IsPanelOpen(0);}

    PanelId CreatePanel(std::wstring title,std::wstring openLabel,
                        std::wstring closeLabel,float width,float height);
    void AddToPanel(PanelId id,std::unique_ptr<UIWidget> widget);
    void StartSecondColumn(PanelId id);
    bool IsPanelOpen(PanelId id)const;
    void Layout(float clientWidth,float clientHeight);
    // 반환값: 마우스 Orbit 허용 여부. 휠은 CameraController에서 독립 처리한다.
    bool Update(const Input& input,float clientWidth,float clientHeight);
    void Render(UIRenderer& renderer)const;
    bool IsMouseCaptured()const{return capturePanel_;}
    bool IsKeyboardCaptured()const{return focused_!=nullptr;}

private:
    struct PanelState
    {
        PanelState(std::wstring name,std::wstring openText,std::wstring closeText,
                   float requestedWidth,float requestedHeight,
                   std::function<void()> onToggle)
            :title(std::move(name)),openLabel(std::move(openText)),
             closeLabel(std::move(closeText)),width(requestedWidth),height(requestedHeight),
             toggle(openLabel,std::move(onToggle)){}
        std::wstring title,openLabel,closeLabel;
        float width=0.f,height=0.f;
        UIPanel panel;
        UIButton toggle;
        std::vector<std::unique_ptr<UIWidget>> widgets;
        std::size_t secondColumnIndex=static_cast<std::size_t>(-1);
        UIRect contentClip{};
        bool open=false,manualPosition=false;
        float x=0.f,y=54.f;
    };
    void TogglePanel(PanelId id);
    void CommitFocusedEdit();
    bool IsOverAnyUI(float x,float y)const;
    std::vector<std::unique_ptr<PanelState>> panels_;
    UIWidget* active_=nullptr;
    UIWidget* focused_=nullptr;
    PanelId captureId_=0;
    bool draggingHeader_=false;
    bool suppressOrbitUntilRelease_=false;
    bool capturePanel_=false;
    float grabOffsetX_=0.f,grabOffsetY_=0.f;
    float mouseX_=0.f,mouseY_=0.f;
    float viewportWidth_=1280.f,viewportHeight_=720.f;
};
