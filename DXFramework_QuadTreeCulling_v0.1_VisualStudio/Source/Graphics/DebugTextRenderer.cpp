// ============================================================================
// DebugTextRenderer.cpp
// ============================================================================

#include "Graphics/DebugTextRenderer.h"

#include <d2d1helper.h>

namespace
{
    std::wstring HResultToString(HRESULT hr)
    {
        wchar_t buffer[32] = {};

        swprintf_s(
            buffer,
            L"0x%08X",
            static_cast<unsigned int>(hr));

        return buffer;
    }
}

bool DebugTextRenderer::Initialize(
    IDXGISwapChain* swapChain)
{
    initialized_ = false;
    lastErrorMessage_.clear();

    if (!swapChain)
    {
        lastErrorMessage_ =
            L"DebugTextRenderer에 전달된 SwapChain이 nullptr입니다.";

        return false;
    }

    swapChain_ =
        swapChain;

    // ------------------------------------------------------------------------
    // 1. Direct2D Factory 생성
    // ------------------------------------------------------------------------
    HRESULT hr =
        D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED,
            d2dFactory_.GetAddressOf());

    if (FAILED(hr))
    {
        lastErrorMessage_ =
            L"Direct2D Factory 생성 실패\nHRESULT: " +
            HResultToString(hr);

        return false;
    }

    // ------------------------------------------------------------------------
    // 2. DirectWrite Factory 생성
    // ------------------------------------------------------------------------
    hr =
        DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(
                dwriteFactory_.GetAddressOf()));

    if (FAILED(hr))
    {
        lastErrorMessage_ =
            L"DirectWrite Factory 생성 실패\nHRESULT: " +
            HResultToString(hr);

        return false;
    }

    // ------------------------------------------------------------------------
    // 3. Debug Text Format
    //
    // 영어 Debug Overlay이므로 Windows 기본 UI Font인 Segoe UI를 사용한다.
    // ------------------------------------------------------------------------
    hr =
        dwriteFactory_->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            18.0f,
            L"en-us",
            textFormat_.GetAddressOf());

    if (FAILED(hr))
    {
        lastErrorMessage_ =
            L"DirectWrite TextFormat 생성 실패\nHRESULT: " +
            HResultToString(hr);

        return false;
    }

    textFormat_->SetTextAlignment(
        DWRITE_TEXT_ALIGNMENT_LEADING);

    textFormat_->SetParagraphAlignment(
        DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    textFormat_->SetWordWrapping(
        DWRITE_WORD_WRAPPING_NO_WRAP);

    // 현재 SwapChain BackBuffer에 Direct2D Target을 연결한다.
    if (!CreateTargetResources())
    {
        return false;
    }

    initialized_ = true;
    return true;
}

void DebugTextRenderer::PrepareForResize()
{
    // ResizeBuffers는 BackBuffer에 대한 모든 외부 참조가 해제되어야 성공한다.
    backgroundBrush_.Reset();
    textBrush_.Reset();
    renderTarget_.Reset();
}

bool DebugTextRenderer::RecreateTarget()
{
    if (!swapChain_ ||
        !d2dFactory_)
    {
        return false;
    }

    return CreateTargetResources();
}

void DebugTextRenderer::DrawTextBlock(
    ID3D11DeviceContext* context,
    const std::wstring& text)
{
    if (!initialized_ ||
        !renderTarget_ ||
        !context ||
        text.empty())
    {
        return;
    }

    // ------------------------------------------------------------------------
    // Direct3D가 현재 BackBuffer에 쓰고 있는 작업을 먼저 제출하고,
    // OM Target을 해제한 뒤 Direct2D가 같은 Surface를 사용하게 한다.
    //
    // Debug Overlay의 단순성과 안정성을 우선한 구조다.
    // ------------------------------------------------------------------------
    context->OMSetRenderTargets(
        0,
        nullptr,
        nullptr);

    context->Flush();

    renderTarget_->BeginDraw();

    renderTarget_->SetTransform(
        D2D1::Matrix3x2F::Identity());

    const D2D1_SIZE_F targetSize =
        renderTarget_->GetSize();

    // 글씨가 배경 Terrain에 묻히지 않도록
    // 좌측 상단에 반투명한 검은 배경 박스를 먼저 그린다.
    const D2D1_RECT_F backgroundRect =
        D2D1::RectF(
            12.0f,
            12.0f,
            520.0f,
            150.0f);

    renderTarget_->FillRoundedRectangle(
        D2D1::RoundedRect(
            backgroundRect,
            7.0f,
            7.0f),
        backgroundBrush_.Get());

    const D2D1_RECT_F textRect =
        D2D1::RectF(
            24.0f,
            20.0f,
            targetSize.width - 20.0f,
            145.0f);

    renderTarget_->DrawTextW(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        textFormat_.Get(),
        textRect,
        textBrush_.Get(),
        D2D1_DRAW_TEXT_OPTIONS_CLIP);

    const HRESULT hr =
        renderTarget_->EndDraw();

    // Device/BackBuffer 변화로 Target 재생성이 필요해진 경우
    // 다음 Frame부터 다시 사용할 수 있게 즉시 Target을 재생성한다.
    if (hr == D2DERR_RECREATE_TARGET)
    {
        PrepareForResize();
        CreateTargetResources();
    }
}

bool DebugTextRenderer::IsInitialized() const
{
    return initialized_;
}

const std::wstring& DebugTextRenderer::GetLastErrorMessage() const
{
    return lastErrorMessage_;
}

bool DebugTextRenderer::CreateTargetResources()
{
    backgroundBrush_.Reset();
    textBrush_.Reset();
    renderTarget_.Reset();

    Microsoft::WRL::ComPtr<IDXGISurface> backBufferSurface;

    HRESULT hr =
        swapChain_->GetBuffer(
            0,
            IID_PPV_ARGS(
                backBufferSurface.GetAddressOf()));

    if (FAILED(hr))
    {
        lastErrorMessage_ =
            L"SwapChain BackBuffer Surface 획득 실패\nHRESULT: " +
            HResultToString(hr);

        return false;
    }

    // ------------------------------------------------------------------------
    // 현재 SwapChain이 출력되는 Window의 실제 DPI를 가져온다.
    //
    // ID2D1Factory::GetDesktopDpi()는 최신 Windows SDK에서 Deprecated이므로
    // Desktop Application에서는 GetDpiForWindow()를 사용한다.
    // ------------------------------------------------------------------------
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};

    HRESULT descResult =
        swapChain_->GetDesc(
            &swapChainDesc);

    if (FAILED(descResult))
    {
        lastErrorMessage_ =
            L"SwapChain 정보를 가져오는데 실패했습니다.\nHRESULT: " +
            HResultToString(descResult);

        return false;
    }

    // DPI를 얻지 못하는 예외 상황에서는 Windows 기본 DPI인 96을 사용한다.
    UINT windowDpi = 96u;

    if (swapChainDesc.OutputWindow)
    {
        windowDpi =
            GetDpiForWindow(
                swapChainDesc.OutputWindow);

        if (windowDpi == 0u)
        {
            windowDpi = 96u;
        }
    }

    const float dpiX =
        static_cast<float>(windowDpi);

    const float dpiY =
        static_cast<float>(windowDpi);

    const D2D1_RENDER_TARGET_PROPERTIES properties =
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(
                DXGI_FORMAT_R8G8B8A8_UNORM,
                D2D1_ALPHA_MODE_IGNORE),
            dpiX,
            dpiY);

    hr =
        d2dFactory_->CreateDxgiSurfaceRenderTarget(
            backBufferSurface.Get(),
            &properties,
            renderTarget_.GetAddressOf());

    if (FAILED(hr))
    {
        lastErrorMessage_ =
            L"Direct2D DXGI Surface RenderTarget 생성 실패\nHRESULT: " +
            HResultToString(hr);

        return false;
    }

    // 밝은 Text.
    hr =
        renderTarget_->CreateSolidColorBrush(
            D2D1::ColorF(
                0.95f,
                0.97f,
                1.0f,
                1.0f),
            textBrush_.GetAddressOf());

    if (FAILED(hr))
    {
        lastErrorMessage_ =
            L"Debug Text Brush 생성 실패\nHRESULT: " +
            HResultToString(hr);

        return false;
    }

    // Text 가독성을 위한 반투명 배경.
    hr =
        renderTarget_->CreateSolidColorBrush(
            D2D1::ColorF(
                0.0f,
                0.0f,
                0.0f,
                0.62f),
            backgroundBrush_.GetAddressOf());

    if (FAILED(hr))
    {
        lastErrorMessage_ =
            L"Debug Background Brush 생성 실패\nHRESULT: " +
            HResultToString(hr);

        return false;
    }

    return true;
}
