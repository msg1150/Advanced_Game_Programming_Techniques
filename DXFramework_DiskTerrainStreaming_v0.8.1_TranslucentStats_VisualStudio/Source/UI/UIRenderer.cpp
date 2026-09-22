// ============================================================================
// UIRenderer.cpp : D3D11 Terrain/MiniMap -> D2D UI -> SwapChain Present 순서.
// D2D Target의 BackBuffer 참조는 ResizeBuffers 전에 반드시 해제한다.
// ============================================================================
#include "UI/UIRenderer.h"
#include <d2d1helper.h>
#include <Windows.h>

namespace
{
    D2D1_RECT_F ToD2D(const UIRect& r)
    {return D2D1::RectF(r.X,r.Y,r.X+r.W,r.Y+r.H);}
    D2D1_COLOR_F ToD2D(UIColor c)
    {return D2D1::ColorF(c.R,c.G,c.B,c.A);}
}

bool UIRenderer::Initialize(IDXGISwapChain* swapChain)
{
    error_.clear();
    if(!swapChain){error_=L"UI: SwapChain nullptr";return false;}
    swapChain_=swapChain;
    HRESULT hr=D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                 factory_.GetAddressOf());
    if(FAILED(hr)){error_=L"UI: Direct2D Factory 실패";return false;}
    hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(writeFactory_.GetAddressOf()));
    if(FAILED(hr)){error_=L"UI: DirectWrite Factory 실패";return false;}
    const auto makeFormat=[this](float fontSize,DWRITE_FONT_WEIGHT weight,
                                Microsoft::WRL::ComPtr<IDWriteTextFormat>& format)
    {
        const HRESULT result=writeFactory_->CreateTextFormat(
            L"Malgun Gothic",nullptr,weight,DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,fontSize,L"ko-kr",format.GetAddressOf());
        if(SUCCEEDED(result))
        {
            format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return SUCCEEDED(result);
    };
    if(!makeFormat(14.f,DWRITE_FONT_WEIGHT_NORMAL,textNormal_) ||
       !makeFormat(12.f,DWRITE_FONT_WEIGHT_NORMAL,textSmall_) ||
       !makeFormat(19.f,DWRITE_FONT_WEIGHT_SEMI_BOLD,textHeading_))
    {error_=L"UI: Text Format 생성 실패";return false;}
    return CreateTarget();
}

void UIRenderer::PrepareForResize()
{
    drawing_=false;
    brush_.Reset();
    target_.Reset();
}

bool UIRenderer::RecreateTarget()
{
    if(!swapChain_ || !factory_)return false;
    return CreateTarget();
}

bool UIRenderer::CreateTarget()
{
    PrepareForResize();
    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    if(FAILED(swapChain_->GetBuffer(0,IID_PPV_ARGS(surface.GetAddressOf()))))
    {error_=L"UI: BackBuffer IDXGISurface 획득 실패";return false;}
    // 고정 96 DPI: Win32 Client Pixel과 D2D DIP를 1:1로 맞춰 HitTest 오차 방지.
    const D2D1_RENDER_TARGET_PROPERTIES properties=D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM,D2D1_ALPHA_MODE_IGNORE),
        96.f,96.f);
    const HRESULT hr=factory_->CreateDxgiSurfaceRenderTarget(
        surface.Get(),&properties,target_.GetAddressOf());
    if(FAILED(hr)){error_=L"UI: DXGI Surface RenderTarget 생성 실패";return false;}
    if(FAILED(target_->CreateSolidColorBrush(
        D2D1::ColorF(1.f,1.f,1.f,1.f),brush_.GetAddressOf())))
    {error_=L"UI: SolidColorBrush 생성 실패";PrepareForResize();return false;}
    return true;
}

bool UIRenderer::Begin(ID3D11DeviceContext* context)
{
    if(!context || !target_ || drawing_)return false;
    // D3D 미니맵이 끝난 다음 BackBuffer를 D2D와 공유한다.
    context->OMSetRenderTargets(0,nullptr,nullptr);
    context->Flush();
    target_->BeginDraw();
    target_->SetTransform(D2D1::Matrix3x2F::Identity());
    drawing_=true;
    return true;
}
void UIRenderer::End()
{
    if(!drawing_ || !target_)return;
    drawing_=false;
    const HRESULT result=target_->EndDraw();
    if(result==D2DERR_RECREATE_TARGET)
    {
        PrepareForResize();
        RecreateTarget();
    }
    else if(FAILED(result))error_=L"UI: Direct2D EndDraw 실패";
}
void UIRenderer::SetBrush(UIColor color)
{
    brush_->SetColor(ToD2D(color));
}
void UIRenderer::FillRect(const UIRect& rect,UIColor color)
{
    if(!drawing_ || !brush_)return;
    SetBrush(color);target_->FillRectangle(ToD2D(rect),brush_.Get());
}
void UIRenderer::StrokeRect(const UIRect& rect,UIColor color,float stroke)
{
    if(!drawing_ || !brush_)return;
    SetBrush(color);target_->DrawRectangle(ToD2D(rect),brush_.Get(),stroke);
}
void UIRenderer::FillRoundRect(const UIRect& rect,UIColor color,float radius)
{
    if(!drawing_ || !brush_)return;
    SetBrush(color);target_->FillRoundedRectangle(
        D2D1::RoundedRect(ToD2D(rect),radius,radius),brush_.Get());
}
void UIRenderer::Line(float x1,float y1,float x2,float y2,UIColor color,float stroke)
{
    if(!drawing_ || !brush_)return;
    SetBrush(color);target_->DrawLine(D2D1::Point2F(x1,y1),D2D1::Point2F(x2,y2),
                                      brush_.Get(),stroke);
}
void UIRenderer::Text(const std::wstring& text,const UIRect& rect,UIColor color,UIFont font)
{
    if(!drawing_ || !brush_ || text.empty())return;
    IDWriteTextFormat* format=(font==UIFont::Heading)?textHeading_.Get():
                            (font==UIFont::Small)?textSmall_.Get():textNormal_.Get();
    if(!format)return;
    SetBrush(color);
    target_->DrawTextW(text.c_str(),static_cast<UINT32>(text.size()),format,
                       ToD2D(rect),brush_.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
