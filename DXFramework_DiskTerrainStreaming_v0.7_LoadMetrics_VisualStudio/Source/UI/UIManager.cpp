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
    // 1280x720: 설정 패널은 상단 625x423, 미니맵은 우하단 그대로 둔다.
    // 좌측 기존 기능 / 우측 신규 Prefetch+Cache 설정의 2열 배치.
    constexpr float panelWidth=625.f;
    const float left=std::max(12.f,clientWidth-panelWidth-12.f);
    constexpr float top=12.f;
    panel_.SetBounds({left,top,panelWidth,423.f});
    constexpr float columnWidth=(panelWidth-36.f)/2.f;
    float y1=top+51.f,y2=top+51.f;
    for(std::size_t i=0;i<widgets_.size();++i)
    {
        const bool second=i>=secondColumnIndex_;
        const float x=left+12.f+(second ? columnWidth+12.f : 0.f);
        float& y=second ? y2:y1;
        widgets_[i]->SetBounds({x,y,columnWidth,widgets_[i]->PreferredHeight()});
        y+=widgets_[i]->PreferredHeight()+2.f;
    }
    (void)clientHeight;
}
bool UIManager::Update(const Input& input,float width,float height)
{
    Layout(width,height);
    mouseX_=static_cast<float>(input.GetMouseX());
    mouseY_=static_cast<float>(input.GetMouseY());
    const bool hovered=panel_.Bounds().Contains(mouseX_,mouseY_);

    // Alt+Tab 등으로 창 Focus를 잃으면 편집 내용 폐기 및 입력 차단 해제.
    if(input.DidLoseFocus())
    {
        if(focused_)focused_->CancelTextEdit();
        focused_=nullptr;
        active_=nullptr;
        capturePanel_=false;
        suppressOrbitUntilRelease_=false;
    }

    if(input.IsLeftMousePressed())
    {
        const bool hadKeyboardFocus=(focused_!=nullptr);
        if(focused_)
        {
            // 다른 곳을 클릭하면 입력값 적용(유효하지 않으면 자동 폐기).
            focused_->CommitTextEdit();
            focused_=nullptr;
        }
        if(hadKeyboardFocus && !hovered)
            suppressOrbitUntilRelease_=true;

        if(hovered)
        {
            capturePanel_=true;
            active_=nullptr;
            for(auto it=widgets_.rbegin();it!=widgets_.rend();++it)
            {
                if((*it)->HitTest(mouseX_,mouseY_))
                {
                    active_=it->get();
                    active_->PointerDown(mouseX_,mouseY_);
                    if(active_->IsTextEditing())focused_=active_;
                    break;
                }
            }
        }
    }
    else if(input.IsLeftMouseDown() && capturePanel_ && active_)
        active_->PointerDrag(mouseX_,mouseY_);

    const bool wasCaptured=capturePanel_ || suppressOrbitUntilRelease_;
    if(input.IsLeftMouseReleased() ||
       (!input.IsLeftMouseDown() && (capturePanel_ || suppressOrbitUntilRelease_)))
    {
        if(active_ && input.IsLeftMouseReleased())active_->PointerUp(mouseX_,mouseY_);
        active_=nullptr;
        capturePanel_=false;
        suppressOrbitUntilRelease_=false;
    }

    // Esc는 변경 취소. Enter는 WM_CHAR의 '\r'로 처리한다.
    if(focused_ && input.IsKeyPressed(VK_ESCAPE))
    {
        focused_->CancelTextEdit();
        focused_=nullptr;
    }
    if(focused_)
    {
        for(wchar_t ch:input.GetTextCharacters())
        {
            focused_->HandleTextCharacter(ch);
            if(!focused_->IsTextEditing())
            {
                focused_=nullptr;
                break;
            }
        }
    }
    return !(hovered || wasCaptured || focused_!=nullptr);
}
void UIManager::Render(UIRenderer& renderer)const
{
    panel_.Render(renderer,L"Terrain Settings");
    for(const auto& widget:widgets_)
        widget->Render(renderer,widget->HitTest(mouseX_,mouseY_) ||
                       (capturePanel_ && widget.get()==active_));
}
