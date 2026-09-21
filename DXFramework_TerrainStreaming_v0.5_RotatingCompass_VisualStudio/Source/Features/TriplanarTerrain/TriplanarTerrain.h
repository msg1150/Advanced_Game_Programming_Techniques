// ============================================================================
// TriplanarTerrain.h
// ----------------------------------------------------------------------------
// 기존 HeightMap + QuadTree LOD + Frustum Culling 구조를 재사용하면서
// Material만 Triplanar Mapping으로 교체한 독립 Terrain Feature.
//
// 기존 QuadTreeLODTerrain은 수정하지 않는다.
//
// 제거 방법:
// - Source/Features/TriplanarTerrain 제거
// - Shaders/TriplanarTerrain 제거
// - Application 연결부 제거
//
// 그러면 이전 QuadTree LOD Terrain은 그대로 남는다.
// ============================================================================

#pragma once

#include "Features/HeightMapTerrain/HeightMapImage.h"
#include "Features/HeightMapTerrain/HeightMapTerrainGenerator.h"
#include "Features/QuadTreeLOD/QuadTreeLOD.h"
#include "Features/QuadTreeLOD/QuadTreeLODSelector.h"
#include "Features/TriplanarTerrain/TriplanarMaterial.h"

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

struct TriplanarTerrainSettings
{
    QuadTreeLODSettings LOD;

    bool EnableCulling = true;
    bool EnableLOD = true;
};

class TriplanarTerrain
{
public:
    bool Initialize(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        const std::filesystem::path& shaderDirectory,
        const std::filesystem::path& heightMapImagePath,
        const HeightMapTerrainSettings& terrainSettings,
        const TriplanarMaterialDesc& materialDesc,
        const TriplanarTerrainSettings& settings);

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

    // 기존 QuadTree LOD 자료구조 / Selector를 그대로 재사용한다.
    QuadTreeLOD quadTreeLOD_;
    QuadTreeLODMeshData lodMeshData_;
    QuadTreeLODSettings lodSettings_;

    Frustum frustum_;

    Mesh mesh_;

    // 기존 TextureSplatMaterial 대신 새 TriplanarMaterial만 사용한다.
    TriplanarMaterial material_;

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
