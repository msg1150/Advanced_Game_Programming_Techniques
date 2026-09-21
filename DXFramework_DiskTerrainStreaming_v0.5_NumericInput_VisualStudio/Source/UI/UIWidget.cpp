// ============================================================================
// UIWidget.cpp : Label / CheckBox / Slider / Panel 그리기와 마우스 반응.
// ============================================================================
#include "UI/UIWidget.h"
#include "UI/UISliderMath.h"
#include "UI/UINumericValue.h"
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

UIRect UISlider::NumberRect()const
{
    // 슬라이더의 우측 숫자를 클릭하면 텍스트 편집이 시작된다.
    return {bounds_.X+bounds_.W-77.f,bounds_.Y+1.f,69.f,24.f};
}

void UISlider::BeginTextEdit()
{
    wchar_t buffer[48]={};
    swprintf_s(buffer,L"%.0f",static_cast<double>(getter_()));
    editText_=buffer;
    selectAll_=true;
    editing_=true;
}

void UISlider::SetFromMouse(float x)
{
    const float left=bounds_.X+10.f;
    const float width=std::max(1.f,bounds_.W-20.f);
    const float candidate=UISliderMath::ValueFromX(x,left,width,min_,max_,step_);
    if(std::abs(candidate-getter_())>0.0001f)setter_(candidate);
}

void UISlider::PointerDown(float x,float y)
{
    if(NumberRect().Contains(x,y))
    {
        BeginTextEdit();
        return;
    }
    editing_=false;
    SetFromMouse(x);
}

void UISlider::PointerDrag(float x,float)
{
    // 숫자 입력칸에서 시작한 Drag가 아래의 슬라이더를 조작하지 않도록 방어.
    if(!editing_)SetFromMouse(x);
}

void UISlider::HandleTextCharacter(wchar_t ch)
{
    if(!editing_)return;
    if(ch==L'\r')
    {
        CommitTextEdit();
        return;
    }
    if(ch==L'\b')
    {
        if(selectAll_)editText_.clear();
        else if(!editText_.empty())editText_.pop_back();
        selectAll_=false;
        return;
    }
    const bool digit=ch>=L'0' && ch<=L'9';
    const bool sign=(ch==L'-'||ch==L'+');
    const bool dot=ch==L'.';
    if(!digit && !sign && !dot)return;
    if(selectAll_)
    {
        editText_.clear();
        selectAll_=false;
    }
    if(editText_.size()>=16)return;
    if(sign && !editText_.empty())return;
    if(dot && editText_.find(L'.')!=std::wstring::npos)return;
    editText_.push_back(ch);
}

void UISlider::CommitTextEdit()
{
    if(!editing_)return;
    float parsed=0.f;
    // 잘못된 입력은 setter 호출 없이 폐기한다. 유효하면 범위로 Clamp.
    if(UINumericValue::ParseClamped(editText_,min_,max_,parsed) &&
       std::abs(parsed-getter_())>0.0001f)
        setter_(parsed);
    editing_=false;
    selectAll_=false;
    editText_.clear();
}

void UISlider::CancelTextEdit()
{
    // Esc, 창 포커스 상실: 원래 Terrain 설정값을 그대로 유지한다.
    editing_=false;
    selectAll_=false;
    editText_.clear();
}

void UISlider::Render(UIRenderer& r,bool hovered)const
{
    const float value=getter_();
    wchar_t buffer[48]={};
    swprintf_s(buffer,L"%.0f",static_cast<double>(value));
    r.Text(label_,{bounds_.X+8.f,bounds_.Y,bounds_.W-87.f,25.f},kText);

    // 텍스트 편집 중에는 강조된 사각형과 입력 버퍼를 출력한다.
    // 실제 Terrain 값은 Enter 또는 입력칸 밖 클릭 전까지 변경하지 않는다.
    const UIRect field=NumberRect();
    r.FillRoundRect(field,editing_?UIColor{.13f,.29f,.29f,1.f}:
                            UIColor{.13f,.19f,.25f,1.f},4.f);
    r.StrokeRect(field,editing_?kAccent:UIColor{.38f,.48f,.56f,1.f},1.f);
    const std::wstring display=editing_?editText_:std::wstring(buffer);
    r.Text(display+(editing_?L"|":L""),
           {field.X+6.f,field.Y+1.f,field.W-9.f,field.H-2.f},
           editing_?kText:kAccent,UIFont::Normal);

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
