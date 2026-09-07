// ============================================================================
// Renderer.h
// ----------------------------------------------------------------------------
// Direct3D 11의 핵심 장치와 Frame Buffer를 관리한다.
//
// Renderer가 담당하는 것:
// - ID3D11Device
// - ID3D11DeviceContext
// - SwapChain
// - RenderTargetView
// - DepthStencil
// - Viewport
// - 기본 Rasterizer State
//
// Mesh나 Camera가 Direct3D 초기화 세부사항을 알지 않도록
// 그래픽 API 기반 코드를 한곳에 모으는 역할을 한다.
// ============================================================================

#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

class Renderer
{
public:
    // Direct3D 11 Device / Context / SwapChain 등을 생성한다.
    bool Initialize(
        HWND hwnd,
        UINT width,
        UINT height);

    // Window 크기가 바뀌었을 때
    // BackBuffer / DepthBuffer / Viewport를 다시 생성한다.
    void Resize(
        UINT width,
        UINT height);

    // RenderTarget / Depth Buffer를 Clear하고 이번 프레임을 시작한다.
    void BeginFrame(
        const float clearColor[4]);

    // 현재 BackBuffer를 화면에 표시한다.
    void EndFrame();

    bool IsInitialized() const;

    // 다른 Graphics 클래스가 GPU 리소스를 생성하거나
    // Pipeline에 바인딩할 때 필요한 Device / Context 접근자.
    ID3D11Device* GetDevice() const;
    ID3D11DeviceContext* GetContext() const;

private:
    // SwapChain의 BackBuffer 크기에 종속적인
    // RenderTarget / DepthStencil / Viewport를 생성한다.
    bool CreateBackBufferResources(
        UINT width,
        UINT height);

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilBuffer_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView_;

    // 현재 기본 Rasterizer:
    // - Solid Fill
    // - Back Face Culling
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState_;

    UINT width_ = 0;
    UINT height_ = 0;

    bool initialized_ = false;
};
