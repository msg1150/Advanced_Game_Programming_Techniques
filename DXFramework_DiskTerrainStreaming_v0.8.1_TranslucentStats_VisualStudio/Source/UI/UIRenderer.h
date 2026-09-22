// ============================================================================
// UIRenderer.h : 프레임워크 공통 2D/UI 그리기 백엔드 (Direct2D+DirectWrite)
// UI 입력/게임 설정/위젯을 전혀 알지 않는다. Pixel 좌표 = 96 DPI DIP 좌표.
// ============================================================================
#pragma once
#include <d2d1.h>
#include <dwrite.h>
#include <dxgi.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
struct UIRect
{
    float X=0.f,Y=0.f,W=0.f,H=0.f;
    bool Contains(float x,float y) const noexcept
    { return x>=X && x<X+W && y>=Y && y<Y+H; }
};
struct UIColor
{
    float R=1.f,G=1.f,B=1.f,A=1.f;
};
enum class UIFont {Small,Normal,Heading};
class UIRenderer
{
public:
    bool Initialize(IDXGISwapChain* swapChain);
    void PrepareForResize();
    bool RecreateTarget();
    bool Begin(ID3D11DeviceContext* context);
    void End();
    void FillRect(const UIRect& rect,UIColor color);
    void StrokeRect(const UIRect& rect,UIColor color,float stroke=1.f);
    void FillRoundRect(const UIRect& rect,UIColor color,float radius=6.f);
    void Line(float x1,float y1,float x2,float y2,UIColor color,float stroke=1.f);
    void Text(const std::wstring& text,const UIRect& rect,UIColor color,
              UIFont font=UIFont::Normal);
    const std::wstring& GetError() const{return error_;}
private:
    bool CreateTarget();
    void SetBrush(UIColor color);
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> writeFactory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> textNormal_,textSmall_,textHeading_;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    bool drawing_=false;
    std::wstring error_;
};
