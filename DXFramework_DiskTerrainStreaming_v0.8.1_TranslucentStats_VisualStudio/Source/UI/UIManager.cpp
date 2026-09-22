// ============================================================================
// UIManager.cpp : 작은 사이드 버튼으로 접기/펼치기, 제목줄 드래그, 입력 라우팅.
// ============================================================================
#include "UI/UIManager.h"
#include "UI/UIPlacement.h"
#include "Input/Input.h"
#include <algorithm>
#include <utility>

namespace
{
    constexpr float kPanelWidth=500.f;
    constexpr float kPanelHeight=425.f;
    constexpr float kPanelHeaderHeight=43.f;
    constexpr float kPanelInitialTop=54.f;
    constexpr float kPanelMargin=12.f;
}

UIManager::UIManager()
    :toggleButton_(L"설정 열기",[this]{TogglePanel();})
{}

void UIManager::Add(std::unique_ptr<UIWidget> widget)
{
    if(widget)widgets_.push_back(std::move(widget));
}

void UIManager::CommitFocusedEdit()
{
    if(!focused_)return;
    focused_->CommitTextEdit();
    focused_=nullptr;
}

void UIManager::TogglePanel()
{
    // 버튼으로 닫을 때 유효한 숫자 수정 내용은 바깥 클릭과 똑같이 적용한다.
    CommitFocusedEdit();
    panelOpen_=!panelOpen_;
    draggingHeader_=false;
    toggleButton_.SetLabel(panelOpen_?L"설정 닫기":L"설정 열기");
}

void UIManager::Layout(float clientWidth,float clientHeight)
{
    viewportWidth_=std::max(0.f,clientWidth);
    viewportHeight_=std::max(0.f,clientHeight);
    const auto tab=UIPlacement::SideButton(viewportWidth_);
    toggleButton_.SetBounds({tab.X,tab.Y,tab.W,tab.H});

    // 너비가 좁아져도 패널의 오른쪽이 창 밖으로 튀지 않도록 제한한다.
    const float panelWidth=std::min(kPanelWidth,std::max(0.f,viewportWidth_-24.f));
    if(!manualPosition_)
    {
        panelX_=std::max(0.f,viewportWidth_-panelWidth-kPanelMargin);
        panelY_=kPanelInitialTop;
    }
    const auto adjusted=UIPlacement::ClampPanel(
        panelX_,panelY_,panelWidth,kPanelHeight,viewportWidth_,viewportHeight_);
    panelX_=adjusted.X;
    panelY_=adjusted.Y;
    panel_.SetBounds({adjusted.X,adjusted.Y,adjusted.W,adjusted.H});

    const float columnWidth=std::max(0.f,(panelWidth-36.f)/2.f);
    float yLeft=panelY_+51.f;
    float yRight=panelY_+51.f;
    for(std::size_t i=0;i<widgets_.size();++i)
    {
        const bool right=i>=secondColumnIndex_;
        float& y=right?yRight:yLeft;
        const float x=panelX_+12.f+(right?columnWidth+12.f:0.f);
        widgets_[i]->SetBounds({x,y,columnWidth,widgets_[i]->PreferredHeight()});
        y+=widgets_[i]->PreferredHeight()+2.f;
    }
}

bool UIManager::Update(const Input& input,float width,float height)
{
    Layout(width,height);
    mouseX_=static_cast<float>(input.GetMouseX());
    mouseY_=static_cast<float>(input.GetMouseY());

    // Alt+Tab: 진행 중인 편집을 취소하고 Drag/Mouse Capture 상태를 해제한다.
    if(input.DidLoseFocus())
    {
        if(focused_)focused_->CancelTextEdit();
        focused_=nullptr;
        active_=nullptr;
        capturePanel_=false;
        draggingHeader_=false;
        suppressOrbitUntilRelease_=false;
    }

    const bool tabHovered=toggleButton_.HitTest(mouseX_,mouseY_);
    const bool panelHovered=panelOpen_ && panel_.Bounds().Contains(mouseX_,mouseY_);

    if(input.IsLeftMousePressed() && !input.DidLoseFocus())
    {
        const bool hadKeyboardFocus=focused_!=nullptr;
        CommitFocusedEdit();
        if(hadKeyboardFocus && !tabHovered && !panelHovered)
            suppressOrbitUntilRelease_=true;

        if(tabHovered)
        {
            // 열린 패널과 겹칠 경우에도 최상단 사이드 버튼이 우선한다.
            capturePanel_=true;
            draggingHeader_=false;
            active_=&toggleButton_;
        }
        else if(panelHovered)
        {
            capturePanel_=true;
            active_=nullptr;
            const UIRect& b=panel_.Bounds();
            if(mouseY_<b.Y+kPanelHeaderHeight)
            {
                draggingHeader_=true;
                grabOffsetX_=mouseX_-b.X;
                grabOffsetY_=mouseY_-b.Y;
            }
            else
            {
                draggingHeader_=false;
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
    }
    else if(input.IsLeftMouseDown() && capturePanel_)
    {
        if(draggingHeader_)
        {
            manualPosition_=true;
            panelX_=mouseX_-grabOffsetX_;
            panelY_=mouseY_-grabOffsetY_;
            Layout(width,height); // 드래그 이동도 창 밖으로 넘어가지 않게 Clamp.
        }
        else if(active_ && active_!=&toggleButton_)
        {
            active_->PointerDrag(mouseX_,mouseY_);
        }
    }

    const bool wasCaptured=capturePanel_ || suppressOrbitUntilRelease_;
    if(input.IsLeftMouseReleased() ||
       (!input.IsLeftMouseDown() && (capturePanel_ || suppressOrbitUntilRelease_)))
    {
        if(active_ && input.IsLeftMouseReleased())active_->PointerUp(mouseX_,mouseY_);
        active_=nullptr;
        capturePanel_=false;
        draggingHeader_=false;
        suppressOrbitUntilRelease_=false;
    }

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

    const bool currentlyHoveringUI=toggleButton_.HitTest(mouseX_,mouseY_) ||
        (panelOpen_ && panel_.Bounds().Contains(mouseX_,mouseY_));
    return !(currentlyHoveringUI || wasCaptured || focused_!=nullptr);
}

void UIManager::Render(UIRenderer& renderer)const
{
    if(panelOpen_)
    {
        panel_.Render(renderer,L"지형 설정");
        const UIRect& b=panel_.Bounds();
        renderer.Text(L"제목줄을 드래그하여 이동",
                      {b.X+b.W-192.f,b.Y+11.f,175.f,23.f},
                      {.64f,.76f,.80f,1.f},UIFont::Small);
        for(const auto& widget:widgets_)
            widget->Render(renderer,widget->HitTest(mouseX_,mouseY_) ||
                           (capturePanel_ && widget.get()==active_));
    }
    // 항상 화면 맨 위에 버튼을 그려, 창을 이동해도 열기/닫기가 가능하다.
    toggleButton_.Render(renderer,toggleButton_.HitTest(mouseX_,mouseY_));
}
