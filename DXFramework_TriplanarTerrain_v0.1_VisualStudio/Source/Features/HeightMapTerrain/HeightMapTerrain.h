// ============================================================================
// HeightMapTerrain.h
// ----------------------------------------------------------------------------
// HeightMap 기반 Terrain Feature의 렌더링 단위를 담당한다.
//
// 이 클래스는 HeightMapTerrainGenerator가 만든 CPU Mesh Data를
// 기존 Framework의 Mesh / Shader / ConstantBuffer 시스템에 연결한다.
//
// 또한 HeightMap Terrain용 Wireframe Rasterizer State를 내부에서만 사용한다.
// Draw 이후에는 원래 Rasterizer State를 복구하므로
// 기존 Renderer 전역 상태를 오염시키지 않는다.
// ============================================================================

#pragma once

#include "Features/HeightMapTerrain/HeightMapTerrainGenerator.h"
#include "Graphics/ConstantBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/Shader.h"
#include "Graphics/Transform.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>

class HeightMapTerrain
{
public:
    bool Initialize(
        ID3D11Device* device,
        const std::filesystem::path& shaderDirectory,
        const std::filesystem::path& imagePath,
        const HeightMapTerrainSettings& settings = {});

    void Render(
        ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection);

    bool IsInitialized() const;
    const std::wstring& GetLastErrorMessage() const;

    Transform& GetTransform();
    const Transform& GetTransform() const;

private:
    struct CBTransform
    {
        DirectX::XMFLOAT4X4 WorldViewProjection;
    };

    bool CreateWireframeRasterizerState(ID3D11Device* device);

private:
    HeightMapImage heightMapImage_;

    Mesh mesh_;
    Shader shader_;
    ConstantBuffer<CBTransform> transformBuffer_;
    Transform transform_;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> wireframeRasterizerState_;

    bool initialized_ = false;
    std::wstring lastErrorMessage_;
};
