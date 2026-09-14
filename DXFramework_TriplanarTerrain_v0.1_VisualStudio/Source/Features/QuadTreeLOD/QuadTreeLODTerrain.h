// ============================================================================
// QuadTreeLODTerrain.h
// ----------------------------------------------------------------------------
// HeightMap + Texture Splatting + Frustum Culling + QuadTree LOD를 결합한
// 독립 Terrain Feature.
//
// 기존 QuadTreeTerrain은 수정하지 않는다.
// 이번 Feature를 제거하면 이전 QuadTree Culling 단계 구현은 그대로 남는다.
// ============================================================================

#pragma once

#include "Features/HeightMapTerrain/HeightMapImage.h"
#include "Features/HeightMapTerrain/HeightMapTerrainGenerator.h"
#include "Features/QuadTreeLOD/QuadTreeLOD.h"
#include "Features/QuadTreeLOD/QuadTreeLODSelector.h"
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

struct QuadTreeLODTerrainSettings
{
    QuadTreeLODSettings LOD;

    bool EnableCulling = true;
    bool EnableLOD = true;
};

class QuadTreeLODTerrain
{
public:
    bool Initialize(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        const std::filesystem::path& shaderDirectory,
        const std::filesystem::path& heightMapImagePath,
        const HeightMapTerrainSettings& terrainSettings,
        const TextureSplatMaterialDesc& materialDesc,
        const QuadTreeLODTerrainSettings& settings);

    void Render(
        ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        const DirectX::XMFLOAT3& cameraWorldPosition);

    void SetCullingEnabled(
        bool enabled);

    bool IsCullingEnabled() const;

    void SetLODEnabled(
        bool enabled);

    bool IsLODEnabled() const;

    const QuadTreeLODStats& GetStats() const;

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

    void BuildFullResolutionOnlyStats();

private:
    HeightMapImage heightMapImage_;

    QuadTreeLOD quadTreeLOD_;
    QuadTreeLODMeshData lodMeshData_;
    QuadTreeLODSettings lodSettings_;

    Frustum frustum_;

    Mesh mesh_;
    TextureSplatMaterial material_;

    ConstantBuffer<CBTransform> transformBuffer_;
    Transform transform_;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> solidRasterizerState_;

    std::vector<QuadTreeLODDrawRange> drawRanges_;

    QuadTreeLODStats stats_;

    bool cullingEnabled_ = true;
    bool lodEnabled_ = true;
    bool initialized_ = false;

    std::wstring lastErrorMessage_;
};
