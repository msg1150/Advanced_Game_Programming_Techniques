// ============================================================================
// TerrainStreamingManager.h
// ----------------------------------------------------------------------------
// 독립 Feature: Chunk GPU 리소스 거리 기반 Load / Unload.
// Worker는 CPU Chunk 생성만, 이 클래스는 Render 스레드에서 GPU 생성/해제만 한다.
// HeightMap 원본은 한 번 읽어서 CPU에 보관한다. 디스크 Tile Streaming은 제외한다.
// ============================================================================
#pragma once
#include "Features/TerrainStreaming/TerrainStreamingWorker.h"
#include "Features/TerrainStreaming/StreamingRangeDebugRenderer.h"
#include "Features/TriplanarTerrain/TriplanarMaterial.h"
#include "Features/QuadTreeLOD/QuadTreeLODSelector.h"
#include "Graphics/ConstantBuffer.h"
#include "Graphics/Frustum.h"
#include "Graphics/Mesh.h"
#include "Graphics/Shader.h"
#include "Graphics/Transform.h"
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct TerrainStreamingSettings
{
    std::uint32_t CellsPerChunk=32u;
    QuadTreeLODSettings LOD={};
    // 지형 크기는 기본 약 6.4 World Unit. 중앙에서는 일부만 로드한다.
    float LoadRadius=1.9f;
    float UnloadRadius=2.9f; // LoadRadius보다 커야 깜빡임 방지.
    std::uint32_t MaxUploadsPerFrame=2u;
    bool EnableStreaming=true;
    bool EnableCulling=true;
    bool EnableLOD=true;
};

struct TerrainStreamingStats
{
    std::uint32_t TotalChunks=0u;
    std::uint32_t DesiredChunks=0u;
    std::uint32_t LoadedChunks=0u;
    std::uint32_t PendingChunks=0u;
    std::uint32_t VisibleChunks=0u;
    std::uint32_t CulledChunks=0u;
    std::uint32_t DrawCalls=0u;
    std::uint32_t RenderedSurfaceTriangles=0u;
    std::uint32_t FullResolutionSurfaceTriangles=0u;
    std::uint64_t ResidentMeshBytes=0u; // 실제 Mesh 버퍼 요청 Byte 수. Driver 총 메모리 아님.
};

class TerrainStreamingManager
{
public:
    ~TerrainStreamingManager();
    TerrainStreamingManager(const TerrainStreamingManager&)=delete;
    TerrainStreamingManager& operator=(const TerrainStreamingManager&)=delete;
    TerrainStreamingManager()=default;

    bool Initialize(ID3D11Device* device,ID3D11DeviceContext* context,
        const std::filesystem::path& heightmapPath,
        const HeightMapTerrainSettings& heightSettings,
        const std::filesystem::path& shaderDirectory,
        const TriplanarMaterialDesc& materialDesc,
        const TerrainStreamingSettings& settings);

    // Camera Target을 입력한다: Orbit Camera의 실제 Position 대신
    // 사용자가 현재 보고/이동하고 있는 Terrain 위치를 Streaming 중심으로 쓴다.
    // 반드시 Render/메인 스레드에서 호출한다(GPU 버퍼 생성/파괴 포함).
    void UpdateStreaming(const DirectX::XMFLOAT3& focusPosition);
    void Render(ID3D11DeviceContext* context,const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,const DirectX::XMFLOAT3& cameraPosition);

    void SetStreamingEnabled(bool enabled);
    void SetCullingEnabled(bool enabled);
    void SetLODEnabled(bool enabled);
    void SetChunkBordersVisible(bool enabled);
    void SetStreamingRangeVisible(bool enabled);
    bool IsStreamingEnabled() const;
    bool IsCullingEnabled() const;
    bool IsLODEnabled() const;
    bool IsChunkBordersVisible() const;
    bool IsStreamingRangeVisible() const;
    Transform& GetTransform();
    const TerrainStreamingStats& GetStats() const;
    const std::wstring& GetLastErrorMessage() const;
private:
    struct Resident
    {
        QuadTreeLOD Tree;
        Mesh GpuMesh;
        Mesh BorderMesh;
        std::vector<QuadTreeLODDrawRange> Ranges;
        QuadTreeLODStats Frame;
        std::uint32_t FullTriangles=0u;
        std::uint32_t FullStartIndex=0u,FullIndexCount=0u;
        std::uint64_t GpuBytes=0u;
        bool Visible=false;
    };
    struct Slot
    {
        StreamingChunkDescriptor Desc;
        std::unique_ptr<Resident> Gpu;
        bool Desired=false, Pending=false, Failed=false;
        std::uint64_t Generation=0u;
    };
    bool CreateRasterizers(ID3D11Device* device);
    bool Install(StreamingCPUResult& result,Slot& slot);
    void RefreshStats();
    static float DistanceXZ(const DirectX::XMFLOAT3& focus,
                            const DirectX::BoundingBox& worldBounds);

    TerrainStreamingWorker worker_;
    std::vector<Slot> slots_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    TriplanarMaterial material_;
    Shader borderShader_;

    struct TransformCB { DirectX::XMFLOAT4X4 WorldViewProjection; DirectX::XMFLOAT4X4 World; };
    ConstantBuffer<TransformCB> transforms_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> solidState_, lineState_;
    Frustum frustum_;
    Transform transform_;
    TerrainStreamingSettings settings_;
    TerrainStreamingStats stats_;

    // F4: 각 Chunk 경계선 표시.
    bool showBorders_=true;

    // F6: Streaming 판정 반경선 표시.
    bool showStreamingRange_=true;

    // 디버그 3D 원기둥 렌더링만 담당하는 독립 클래스.
    StreamingRangeDebugRenderer rangeDebugRenderer_;

    bool initialized_=false;
    std::wstring lastError_;
};
