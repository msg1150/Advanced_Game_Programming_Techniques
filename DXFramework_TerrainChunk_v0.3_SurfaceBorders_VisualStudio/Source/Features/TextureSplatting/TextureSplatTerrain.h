// ============================================================================
// TextureSplatTerrain.h
// ----------------------------------------------------------------------------
// 기존 HeightMap Geometry 생성 코드를 재사용하고,
// 높이 / 경사도 기반 자동 Texture Splat Material을 적용하는 Terrain Feature.
//
// 기존 HeightMapTerrain 자체는 수정하지 않는다.
// 따라서 Texture Splatting 기능을 제거하더라도 이전 단계 구현은 유지된다.
// ============================================================================

#pragma once

#include "Features/HeightMapTerrain/HeightMapImage.h"
#include "Features/HeightMapTerrain/HeightMapTerrainGenerator.h"
#include "Features/TextureSplatting/TextureSplatMaterial.h"

#include "Graphics/ConstantBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/Transform.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>

class TextureSplatTerrain
{
public:
    bool Initialize(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        const std::filesystem::path& shaderDirectory,
        const std::filesystem::path& heightMapImagePath,
        const HeightMapTerrainSettings& terrainSettings,
        const TextureSplatMaterialDesc& materialDesc);

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
        // Clip Space 변환용.
        DirectX::XMFLOAT4X4 WorldViewProjection;

        // Pixel Shader가 사용할 World Height / World Normal 계산용.
        DirectX::XMFLOAT4X4 World;
    };

    bool CreateSolidRasterizerState(
        ID3D11Device* device);

private:
    HeightMapImage heightMapImage_;

    Mesh mesh_;
    TextureSplatMaterial material_;

    ConstantBuffer<CBTransform> transformBuffer_;
    Transform transform_;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> solidRasterizerState_;

    bool initialized_ = false;
    std::wstring lastErrorMessage_;
};
