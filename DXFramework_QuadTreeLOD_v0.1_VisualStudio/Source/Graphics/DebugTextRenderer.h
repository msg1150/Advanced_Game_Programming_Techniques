// ============================================================================
// DebugTextRenderer.h
// ----------------------------------------------------------------------------
// Direct3D 11 BackBuffer 위에 간단한 Debug Text Overlay를 그리는 공용 클래스.
//
// 사용 기술:
// - Direct2D  : 2D Overlay 렌더링
// - DirectWrite: Font / Text 출력
//
// QuadTree Culling의 ON/OFF 상태와 통계를 화면에서 바로 확인하기 위해
// 추가했지만, 클래스 자체는 특정 Feature에 종속되지 않는다.
// ============================================================================

#pragma once

#include <d2d1.h>
#include <dwrite.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <d3d11.h>

#include <string>

class DebugTextRenderer
{
public:
    // SwapChain BackBuffer를 대상으로 Direct2D RenderTarget을 생성한다.
    bool Initialize(
        IDXGISwapChain* swapChain);

    // ResizeBuffers 전에 BackBuffer를 참조하는 Direct2D 리소스를 해제한다.
    void PrepareForResize();

    // ResizeBuffers가 끝난 뒤 새 BackBuffer로 RenderTarget을 다시 만든다.
    bool RecreateTarget();

    // 여러 줄의 Debug Text를 화면 좌측 상단에 출력한다.
    void DrawTextBlock(
        ID3D11DeviceContext* context,
        const std::wstring& text);

    bool IsInitialized() const;

    const std::wstring& GetLastErrorMessage() const;

private:
    bool CreateTargetResources();

private:
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;

    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat_;

    Microsoft::WRL::ComPtr<ID2D1RenderTarget> renderTarget_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> textBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> backgroundBrush_;

    bool initialized_ = false;
    std::wstring lastErrorMessage_;
};
