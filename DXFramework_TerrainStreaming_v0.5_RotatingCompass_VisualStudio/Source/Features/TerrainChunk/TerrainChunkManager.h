// ============================================================================
// TerrainChunkManager.h
// Chunk 단위 Mesh/Bounds/QuadTree를 소유하고 Render하는 독립 Feature.
// 기존 HeightMap, QuadTreeLOD, TriplanarMaterial 코드는 수정하지 않는다.
// Streaming(비동기 로드/언로드)은 이번 단계 범위에서 제외한다.
// ============================================================================
#pragma once
#include "Features/TerrainChunk/TerrainChunkGenerator.h"
#include "Features/QuadTreeLOD/QuadTreeLOD.h"
#include "Features/QuadTreeLOD/QuadTreeLODSelector.h"
#include "Features/TriplanarTerrain/TriplanarMaterial.h"
#include "Graphics/ConstantBuffer.h"
#include "Graphics/Frustum.h"
#include "Graphics/Mesh.h"
#include "Graphics/Shader.h"
#include "Graphics/Transform.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct TerrainChunkSettings
{
    std::uint32_t CellsPerChunk = 32u;
    QuadTreeLODSettings LOD = {};
    bool EnableCulling = true;
    bool EnableLOD = true;
};

struct TerrainChunkStats
{
    std::uint32_t TotalChunks = 0u;
    std::uint32_t VisibleChunks = 0u;
    std::uint32_t CulledChunks = 0u;
    std::uint32_t ActiveNodes = 0u;
    std::uint32_t DrawCalls = 0u;
    std::uint32_t RenderedSurfaceTriangles = 0u;
    std::uint32_t FullResolutionSurfaceTriangles = 0u;
    std::uint32_t ActualDrawTriangles = 0u; // Skirt까지 포함한 실제 Draw 범위
};

class TerrainChunkManager
{
public:
    bool Initialize(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        const std::filesystem::path& heightMapPath,
        const HeightMapTerrainSettings& heightSettings,
        const std::filesystem::path& shaderDirectory,
        const TriplanarMaterialDesc& materialDesc,
        const TerrainChunkSettings& settings);

    void Render(
        ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        const DirectX::XMFLOAT3& cameraPosition);

    void SetCullingEnabled(bool enabled);
    void SetLODEnabled(bool enabled);
    void SetChunkBoundsVisible(bool visible);
    bool IsCullingEnabled() const;
    bool IsLODEnabled() const;
    bool IsChunkBoundsVisible() const;
    Transform& GetTransform();
    const TerrainChunkStats& GetStats() const;
    const std::wstring& GetLastErrorMessage() const;

private:
    struct CBTransform
    {
        DirectX::XMFLOAT4X4 WorldViewProjection;
        DirectX::XMFLOAT4X4 World;
    };
    struct Chunk
    {
        std::uint32_t X = 0u;
        std::uint32_t Z = 0u;
        QuadTreeLOD Tree;
        QuadTreeLODMeshData MeshData;
        Mesh GpuMesh;

        // Chunk 경계를 시각적으로 확인하기 위한 상단 외곽선 Mesh.
        Mesh BoundsMesh;

        std::vector<QuadTreeLODDrawRange> Ranges;
        QuadTreeLODStats FrameStats;

        // 이번 Frame에 실제로 렌더링되었는지 추적한다.
        bool VisibleThisFrame = false;
    };

    bool CreateRasterizers(ID3D11Device* device);
    // HeightMap에서 복사한 Chunk 원본 Vertex의 실제 높이를 따라
    // 외곽선 Line List를 생성한다. Bounds.MaxY 평면은 사용하지 않는다.
    // 인접 Chunk가 같은 Edge를 2번 그려 색상 충돌을 일으키지 않도록
    // +X/+Z 쪽 바깥 테두리는 마지막 열/행에서만 그린다.
    bool CreateChunkBoundsMesh(
        ID3D11Device* device,
        const TerrainChunkMeshData& cpuChunk,
        bool drawLastXEdge,
        bool drawLastZEdge,
        Mesh& outMesh);
    std::vector<std::unique_ptr<Chunk>> chunks_;
    TriplanarMaterial material_;

    // Chunk 경계선은 기본 Color Shader로 그린다.
    Shader chunkBoundsShader_;

    ConstantBuffer<CBTransform> transformBuffer_;

    // 실제 Terrain Surface 렌더링용 Rasterizer.
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_;

    // Chunk 경계선 표시용 Rasterizer.
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> lineRasterizer_;

    Frustum frustum_;
    Transform transform_;
    TerrainChunkSettings settings_;
    TerrainChunkStats stats_;

    // F4로 토글하는 Chunk 경계 Debug 표시 상태.
    bool showChunkBounds_ = true;

    bool initialized_ = false;
    std::wstring lastErrorMessage_;
};
