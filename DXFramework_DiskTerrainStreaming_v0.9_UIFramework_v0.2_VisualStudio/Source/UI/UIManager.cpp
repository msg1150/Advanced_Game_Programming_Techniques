// UIManager.cpp : 다중 패널의 순서/입력 점유/포커스를 한 곳에서 관리.
// 레이아웃은 UILayout, 화면 제한은 UIPlacement, 스타일은 UITheme에서 결정.
#include "UI/UIManager.h"
#include "UI/UILayout.h"
#include "UI/UIPlacement.h"
#include "UI/UITheme.h"
#include "Input/Input.h"
#include <algorithm>
#include <utility>

UIManager::UIManager()
{
    CreatePanel(L"지형 설정",L"설정 열기",L"설정 닫기",500.f,425.f);
}

UIManager::PanelId UIManager::CreatePanel(std::wstring title,std::wstring openLabel,
                                         std::wstring closeLabel,float width,float height)
{
    const PanelId id=panels_.size();
    panels_.push_back(std::make_unique<PanelState>(
        std::move(title),std::move(openLabel),std::move(closeLabel),width,height,
        [this,id]{TogglePanel(id);}));
    // 벡터 원소는 unique_ptr로 관리하여 PanelState/Widget 주소를 유지한다.
    return id;
}

void UIManager::AddToPanel(PanelId id,std::unique_ptr<UIWidget> widget)
{
    if(widget && id<panels_.size())panels_[id]->widgets.push_back(std::move(widget));
}

void UIManager::StartSecondColumn(PanelId id)
{
    if(id<panels_.size())panels_[id]->secondColumnIndex=panels_[id]->widgets.size();
}

bool UIManager::IsPanelOpen(PanelId id)const
{
    return id<panels_.size() && panels_[id]->open;
}

void UIManager::CommitFocusedEdit()
{
    if(!focused_)return;
    focused_->CommitTextEdit();
    focused_=nullptr;
}

void UIManager::TogglePanel(PanelId id)
{
    if(id>=panels_.size())return;
    CommitFocusedEdit();
    auto& p=*panels_[id];
    p.open=!p.open;
    p.toggle.SetLabel(p.open?p.closeLabel:p.openLabel);
    draggingHeader_=false;
}

void UIManager::Layout(float clientWidth,float clientHeight)
{
    viewportWidth_=std::max(0.f,clientWidth);
    viewportHeight_=std::max(0.f,clientHeight);
    for(PanelId i=0;i<panels_.size();++i)
    {
        auto& p=*panels_[i];
        const auto tab=UIPlacement::SideButtonIndexed(viewportWidth_,i,panels_.size());
        p.toggle.SetBounds({tab.X,tab.Y,tab.W,tab.H});

        const float panelWidth=std::min(p.width,std::max(0.f,viewportWidth_-24.f));
        const float panelHeight=std::min(p.height,std::max(0.f,viewportHeight_-16.f));
        if(!p.manualPosition)
        {
            p.x=std::max(0.f,viewportWidth_-panelWidth-UITheme::PanelMargin-
                static_cast<float>(i)*512.f);
            p.y=54.f;
        }
        const auto adjusted=UIPlacement::ClampPanel(
            p.x,p.y,panelWidth,panelHeight,viewportWidth_,viewportHeight_);
        p.x=adjusted.X;
        p.y=adjusted.Y;
        p.panel.SetBounds({adjusted.X,adjusted.Y,adjusted.W,adjusted.H});

        std::vector<float> heights;
        heights.reserve(p.widgets.size());
        for(const auto& widget:p.widgets)heights.push_back(widget->PreferredHeight());
        const UILayout::Result result=UILayout::Arrange(
            p.panel.Bounds(),heights,p.secondColumnIndex);
        p.contentClip=result.ContentClip;
        for(std::size_t w=0;w<p.widgets.size();++w)
            p.widgets[w]->SetBounds(result.Items[w]);
    }
}

bool UIManager::IsOverAnyUI(float x,float y)const
{
    for(const auto& p:panels_)
        if(p->toggle.HitTest(x,y) || (p->open && p->panel.Bounds().Contains(x,y)))
            return true;
    return false;
}

bool UIManager::Update(const Input& input,float width,float height)
{
    Layout(width,height);
    mouseX_=static_cast<float>(input.GetMouseX());
    mouseY_=static_cast<float>(input.GetMouseY());

    if(input.DidLoseFocus())
    {
        if(focused_)focused_->CancelTextEdit();
        focused_=nullptr;
        active_=nullptr;
        capturePanel_=false;
        draggingHeader_=false;
        suppressOrbitUntilRelease_=false;
    }

    if(input.IsLeftMousePressed() && !input.DidLoseFocus())
    {
        const bool hadKeyboardFocus=focused_!=nullptr;
        CommitFocusedEdit();
        if(hadKeyboardFocus && !IsOverAnyUI(mouseX_,mouseY_))
            suppressOrbitUntilRelease_=true;
        // 모든 패널보다 버튼을 위에 그리므로 입력도 버튼이 우선한다.
        bool selected=false;
        for(PanelId reverse=panels_.size();reverse>0;--reverse)
        {
            const PanelId id=reverse-1;
            if(panels_[id]->toggle.HitTest(mouseX_,mouseY_))
            {
                capturePanel_=true;
                captureId_=id;
                draggingHeader_=false;
                active_=&panels_[id]->toggle;
                selected=true;
                break;
            }
        }
        if(!selected)
        {
            // 뒤에 그리는 Panel일수록 전면. 다른 Panel이 아래에 있어도
            // 전면 Panel의 빈 공간을 클릭하면 뒤쪽 Widget이 눌리지 않는다.
            for(PanelId reverse=panels_.size();reverse>0;--reverse)
            {
                const PanelId id=reverse-1;
                auto& p=*panels_[id];
                if(!p.open || !p.panel.Bounds().Contains(mouseX_,mouseY_))continue;
                capturePanel_=true;
                captureId_=id;
                active_=nullptr;
                if(mouseY_<p.panel.Bounds().Y+UITheme::PanelHeaderHeight)
                {
                    draggingHeader_=true;
                    grabOffsetX_=mouseX_-p.panel.Bounds().X;
                    grabOffsetY_=mouseY_-p.panel.Bounds().Y;
                }
                else
                {
                    draggingHeader_=false;
                    if(p.contentClip.Contains(mouseX_,mouseY_))
                    {
                        for(auto it=p.widgets.rbegin();it!=p.widgets.rend();++it)
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
                break;
            }
        }
    }
    else if(input.IsLeftMouseDown() && capturePanel_)
    {
        if(draggingHeader_)
        {
            auto& p=*panels_[captureId_];
            p.manualPosition=true;
            p.x=mouseX_-grabOffsetX_;
            p.y=mouseY_-grabOffsetY_;
            Layout(width,height);
        }
        else if(active_ && active_!=&panels_[captureId_]->toggle)
            active_->PointerDrag(mouseX_,mouseY_);
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
        for(wchar_t ch:input.GetTextCharacters())
        {
            focused_->HandleTextCharacter(ch);
            if(!focused_->IsTextEditing())
            {
                focused_=nullptr;
                break;
            }
        }
    return !(IsOverAnyUI(mouseX_,mouseY_) || wasCaptured || focused_!=nullptr);
}

void UIManager::Render(UIRenderer& renderer)const
{
    // Hover도 실제 Pointer 입력과 동일한 Z-order를 사용한다.
    const PanelState* topHovered=nullptr;
    for(auto it=panels_.rbegin();it!=panels_.rend();++it)
        if((*it)->open && (*it)->panel.Bounds().Contains(mouseX_,mouseY_))
        {
            topHovered=it->get();
            break;
        }
    for(const auto& p:panels_)
    {
        if(!p->open)continue;
        p->panel.Render(renderer,p->title);
        const UIRect& b=p->panel.Bounds();
        // 안내 문구는 기본 Terrain 패널에만 표시하고 일반 Panel에는 강제하지 않는다.
        if(p.get()==panels_.front().get() && b.W>=240.f)
            renderer.Text(L"제목줄을 드래그하여 이동",
                          {b.X+b.W-192.f,b.Y+11.f,175.f,23.f},
                          UITheme::Muted,UIFont::Small);
        if(p->contentClip.W>0.f && p->contentClip.H>0.f)
        {
            renderer.PushClip(p->contentClip);
            for(const auto& widget:p->widgets)
                if(UILayout::Intersects(widget->Bounds(),p->contentClip))
                    widget->Render(renderer,
                        (p.get()==topHovered && p->contentClip.Contains(mouseX_,mouseY_) &&
                         widget->HitTest(mouseX_,mouseY_)) ||
                        (capturePanel_ && widget.get()==active_));
            renderer.PopClip();
        }
    }
    // 패널보다 위에 출력하므로 열린 패널이 버튼을 가리지 않는다.
    for(const auto& p:panels_)
        p->toggle.Render(renderer,p->toggle.HitTest(mouseX_,mouseY_));
}
