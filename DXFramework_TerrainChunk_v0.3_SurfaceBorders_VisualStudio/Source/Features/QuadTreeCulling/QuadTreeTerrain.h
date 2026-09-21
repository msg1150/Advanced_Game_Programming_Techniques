// ============================================================================
// QuadTreeTerrain.h
// ----------------------------------------------------------------------------
// 현재까지 만든 HeightMap + Height/Slope Texture Splatting Terrain에
// QuadTree Frustum Culling을 적용하는 최종 Showcase Feature.
//
// 기존 기능 재사용:
// - HeightMapImage
// - HeightMapTerrainGenerator
// - TextureSplatMaterial
// - Mesh
//
// 새 기능:
// - QuadTree
// - Frustum
// - Visible Index Range Draw
// ============================================================================

#pragma once

#include "Features/HeightMapTerrain/HeightMapImage.h"
#include "Features/HeightMapTerrain/HeightMapTerrainGenerator.h"
#include "Features/QuadTreeCulling/QuadTree.h"
#include "Features/QuadTreeCulling/QuadTreeCulling.h"
#include "Features/TextureSplatting/TextureSplatMaterial.h"

#include "Graphics/ConstantBuffer.h"
#include "Graphics/Frustum.h"
#include "Graphics/Mesh.h"
#include "Graphics/Transform.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>
#include <vector>

struct QuadTreeTerrainSettings
{
    QuadTreeSettings QuadTree;

    bool EnableCulling = true;
};

class QuadTreeTerrain
{
public:
    bool Initialize(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        const std::filesystem::path& shaderDirectory,
        const std::filesystem::path& heightMapImagePath,
        const HeightMapTerrainSettings& terrainSettings,
        const TextureSplatMaterialDesc& materialDesc,
        const QuadTreeTerrainSettings& quadTreeSettings);

    void Render(
        ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection);

    void SetCullingEnabled(
        bool enabled);

    bool IsCullingEnabled() const;

    const QuadTreeCullingStats& GetCullingStats() const;

    const std::wstring& GetLastErrorMessage() const;

    Transform& GetTransform();
    const Transform& GetTransform() const;

private:
    struct CBTransform
    {
        DirectX::XMFLOAT4X4 WorldViewProjection;
        DirectX::XMFLOAT4X4 World;
    };

    bool CreateSolidRasterizerState(
        ID3D11Device* device);

private:
    HeightMapImage heightMapImage_;

    QuadTree quadTree_;
    Frustum frustum_;

    Mesh mesh_;
    TextureSplatMaterial material_;

    ConstantBuffer<CBTransform> transformBuffer_;
    Transform transform_;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> solidRasterizerState_;

    std::vector<QuadTreeDrawRange> visibleRanges_;

    QuadTreeCullingStats cullingStats_;

    bool cullingEnabled_ = true;
    bool initialized_ = false;

    std::wstring lastErrorMessage_;
};
