// Only for portable UI tests. Real builds include Source/UI/UIRenderer.h.
#pragma once
#include <string>
#include <vector>
struct UIRect
{
    float X=0.f,Y=0.f,W=0.f,H=0.f;
    bool Contains(float x,float y)const noexcept
    {return x>=X && x<X+W && y>=Y && y<Y+H;}
};
struct UIColor {float R=1.f,G=1.f,B=1.f,A=1.f;};
enum class UIFont {Small,Normal,Heading};
struct UIRenderer
{
    std::vector<std::wstring> text;
    int clipDepth=0;
    void PushClip(const UIRect&){++clipDepth;}
    void PopClip(){--clipDepth;}
    void Text(const std::wstring& value,const UIRect&,UIColor,UIFont=UIFont::Normal)
    {text.push_back(value);}
    void FillRoundRect(const UIRect&,UIColor,float=6.f){}
    void StrokeRect(const UIRect&,UIColor,float=1.f){}
    void Line(float,float,float,float,UIColor,float=1.f){}
};
