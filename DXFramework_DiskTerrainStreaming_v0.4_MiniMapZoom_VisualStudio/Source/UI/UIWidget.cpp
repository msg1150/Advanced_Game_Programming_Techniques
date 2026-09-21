// ============================================================================
// UIWidget.cpp : Label / CheckBox / Slider / Panel 그리기와 마우스 반응.
// ============================================================================
#include "UI/UIWidget.h"
#include "UI/UISliderMath.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cstdio>
#include <utility>
namespace
{
    constexpr UIColor kText={.91f,.95f,.98f,1.f};
    constexpr UIColor kMuted={.65f,.73f,.80f,1.f};
    constexpr UIColor kAccent={.18f,.79f,.61f,1.f};
}
void UILabel::Render(UIRenderer& r,bool)const
{
    r.Text(getter_ ? getter_() : text_,
           {bounds_.X,bounds_.Y,bounds_.W,bounds_.H},kMuted,UIFont::Small);
}
UICheckBox::UICheckBox(std::wstring label,std::function<bool()> getter,
                       std::function<void(bool)> setter)
    :label_(std::move(label)),getter_(std::move(getter)),setter_(std::move(setter)){}
void UICheckBox::Render(UIRenderer& r,bool hovered)const
{
    if(hovered)r.FillRoundRect(bounds_,{.18f,.24f,.31f,.9f},4.f);
    const float left=bounds_.X+7.f,top=bounds_.Y+5.f;
    const UIRect square={left,top,18.f,18.f};
    const bool checked=getter_();
    r.FillRoundRect(square,checked?kAccent:UIColor{.20f,.27f,.34f,1.f},3.f);
    r.StrokeRect(square,checked?kAccent:UIColor{.48f,.55f,.64f,1.f},1.f);
    if(checked)
    {
        r.Line(left+4.f,top+9.f,left+8.f,top+13.f,{.06f,.15f,.14f,1.f},2.f);
        r.Line(left+8.f,top+13.f,left+15.f,top+5.f,{.06f,.15f,.14f,1.f},2.f);
    }
    r.Text(label_,{left+28.f,bounds_.Y+1.f,bounds_.W-42.f,26.f},kText);
}
void UICheckBox::PointerUp(float x,float y)
{
    if(HitTest(x,y))setter_(!getter_());
}
UISlider::UISlider(std::wstring label,float minimum,float maximum,float step,
                  std::function<float()> getter,std::function<void(float)> setter)
    :label_(std::move(label)),min_(minimum),max_(maximum),step_(step),
     getter_(std::move(getter)),setter_(std::move(setter)){}
void UISlider::SetFromMouse(float x)
{
    const float left=bounds_.X+10.f;
    const float width=std::max(1.f,bounds_.W-20.f);
    const float candidate=UISliderMath::ValueFromX(x,left,width,min_,max_,step_);
    if(std::abs(candidate-getter_())>0.0001f)setter_(candidate);
}
void UISlider::PointerDown(float x,float){SetFromMouse(x);}
void UISlider::PointerDrag(float x,float){SetFromMouse(x);}
void UISlider::Render(UIRenderer& r,bool hovered)const
{
    const float value=getter_();
    wchar_t buffer[48]={};
    swprintf_s(buffer,L"%.0f",static_cast<double>(value));
    r.Text(label_,{bounds_.X+8.f,bounds_.Y,bounds_.W-74.f,25.f},kText);
    r.Text(buffer,{bounds_.X+bounds_.W-58.f,bounds_.Y,52.f,25.f},kAccent);
    const float x=bounds_.X+10.f,y=bounds_.Y+39.f,width=std::max(1.f,bounds_.W-20.f);
    r.FillRoundRect({x,y,width,7.f},{.24f,.32f,.39f,1.f},3.f);
    const float fraction=UISliderMath::Fraction(value,min_,max_);
    r.FillRoundRect({x,y,std::max(2.f,width*fraction),7.f},kAccent,3.f);
    const float knob=x+width*fraction;
    r.FillRoundRect({knob-5.f,y-5.f,10.f,17.f},
                    hovered?UIColor{.85f,1.f,.93f,1.f}:UIColor{.80f,.94f,.90f,1.f},4.f);
    wchar_t leftLabel[32]={},rightLabel[32]={};
    swprintf_s(leftLabel,L"min %.0f",static_cast<double>(min_));
    swprintf_s(rightLabel,L"max %.0f",static_cast<double>(max_));
    r.Text(leftLabel,{bounds_.X+9.f,bounds_.Y+49.f,85.f,15.f},kMuted,UIFont::Small);
    r.Text(rightLabel,{bounds_.X+bounds_.W-79.f,bounds_.Y+49.f,75.f,15.f},
           kMuted,UIFont::Small);
}
void UIPanel::Render(UIRenderer& renderer,const std::wstring& title)const
{
    renderer.FillRoundRect(bounds_,{.065f,.09f,.13f,.95f},10.f);
    renderer.StrokeRect(bounds_,{.29f,.38f,.46f,1.f},1.f);
    renderer.Text(title,{bounds_.X+18.f,bounds_.Y+9.f,bounds_.W-34.f,37.f},
                  kText,UIFont::Heading);
    renderer.Line(bounds_.X+13.f,bounds_.Y+44.f,bounds_.X+bounds_.W-13.f,
                  bounds_.Y+44.f,{.23f,.32f,.39f,1.f});
}
