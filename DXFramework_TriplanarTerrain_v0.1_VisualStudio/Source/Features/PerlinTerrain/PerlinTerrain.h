// ============================================================================
// PerlinTerrain.h
// ----------------------------------------------------------------------------
// Perlin Terrain Feature의 GPU 렌더링 단위를 담당한다.
//
// 이 클래스가 소유하는 것:
// - 생성된 Terrain Mesh
// - Terrain 전용 Shader
// - Transform Constant Buffer
// - Terrain 전용 Wireframe Rasterizer State
//
// 중요한 설계:
// Renderer 자체에는 Terrain 전용 기능을 추가하지 않는다.
// Wireframe State도 이 Feature 내부에서 만들고 Render 후 원래 상태로 복구한다.
//
// 따라서 Perlin Terrain 기능을 제거할 때
// Graphics/Renderer, Mesh, Shader 코드를 되돌릴 필요가 없다.
// ============================================================================

#pragma once

#include "Features/PerlinTerrain/PerlinTerrainGenerator.h"
#include "Graphics/ConstantBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/Shader.h"
#include "Graphics/Transform.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>

class PerlinTerrain
{
public:
    // Terrain Feature에 필요한 GPU/CPU 리소스를 생성한다.
    //
    // shaderDirectory 예:
    // .../Shaders/PerlinTerrain
    bool Initialize(
        ID3D11Device* device,
        const std::filesystem::path& shaderDirectory,
        const PerlinTerrainSettings& settings = {});

    // 기존 Framework가 계산한 View / Projection Matrix를 그대로 받아 렌더링한다.
    //
    // Camera 클래스 자체에는 의존하지 않기 때문에
    // 이후 다른 Camera 구현으로 바뀌어도 이 클래스는 영향을 덜 받는다.
    void Render(
        ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection);

    bool IsInitialized() const;

    // 초기화 실패 시 Application 진단 팝업에 전달할 상세 메시지.
    const std::wstring& GetLastErrorMessage() const;

    // 이후 Terrain 위치를 조정할 필요가 있을 때만 사용한다.
    Transform& GetTransform();
    const Transform& GetTransform() const;

private:
    struct CBTransform
    {
        DirectX::XMFLOAT4X4 WorldViewProjection;
    };

    // Terrain 전용 Wireframe State를 생성한다.
    bool CreateWireframeRasterizerState(
        ID3D11Device* device);

private:
    Mesh mesh_;
    Shader shader_;

    ConstantBuffer<CBTransform> transformBuffer_;

    Transform transform_;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState>
        wireframeRasterizerState_;

    bool initialized_ = false;

    std::wstring lastErrorMessage_;
};
