// ============================================================================
// UIManager.cpp : 위젯에 입력 라우팅, 패널 위 마우스를 카메라로 보내지 않는다.
// ============================================================================
#include "UI/UIManager.h"
#include "Input/Input.h"
#include <algorithm>
#include <utility>
void UIManager::Add(std::unique_ptr<UIWidget> widget)
{
    if(widget)widgets_.push_back(std::move(widget));
}
void UIManager::Layout(float clientWidth,float clientHeight)
{
    constexpr float width=305.f;
    const float left=std::max(12.f,clientWidth-width-12.f);
    const float top=12.f;
    // 기본 창 1280x720에서 미니맵(우하단)과 겹치지 않는 높이.
    // Compact Checkbox + 2 Sliders, 미니맵(우하단) 위에 모두 수용.
    // 기본 1280x720에서 패널 bottom=435, 미니맵 top=440.
    const UIRect rect={left,top,width,423.f};
    panel_.SetBounds(rect);
    float y=top+51.f;
    for(auto& widget:widgets_)
    {
        widget->SetBounds({left+12.f,y,width-24.f,widget->PreferredHeight()});
        y+=widget->PreferredHeight()+2.f;
    }
    (void)clientHeight;
}
bool UIManager::Update(const Input& input,float width,float height)
{
    Layout(width,height);
    mouseX_=static_cast<float>(input.GetMouseX());
    mouseY_=static_cast<float>(input.GetMouseY());
    const bool hovered=panel_.Bounds().Contains(mouseX_,mouseY_);
    if(input.IsLeftMousePressed() && hovered)
    {
        capturePanel_=true;
        active_=nullptr;
        for(auto it=widgets_.rbegin();it!=widgets_.rend();++it)
        {
            if((*it)->HitTest(mouseX_,mouseY_))
            {
                active_=it->get();
                active_->PointerDown(mouseX_,mouseY_);
                break;
            }
        }
    }
    else if(input.IsLeftMouseDown() && capturePanel_ && active_)
        active_->PointerDrag(mouseX_,mouseY_);

    const bool wasCaptured=capturePanel_;
    if(input.IsLeftMouseReleased() || (!input.IsLeftMouseDown() && capturePanel_))
    {
        if(active_ && input.IsLeftMouseReleased())active_->PointerUp(mouseX_,mouseY_);
        active_=nullptr;
        capturePanel_=false;
    }
    return !(hovered || wasCaptured);
}
void UIManager::Render(UIRenderer& renderer)const
{
    panel_.Render(renderer,L"Terrain Settings");
    for(const auto& widget:widgets_)
        widget->Render(renderer,widget->HitTest(mouseX_,mouseY_) ||
                       (capturePanel_ && widget.get()==active_));
}
