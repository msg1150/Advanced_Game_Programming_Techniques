// ============================================================================
// DiskTerrainManager.h — Tile Metadata -> 비동기 디스크 파일 -> CPU -> GPU.
// 지난 프로젝트의 GPU-only Streaming을 유지하지 않고 새 Feature로 분리한다.
// ============================================================================
#pragma once
#include "Features/DiskTerrainStreaming/DiskTileWorker.h"
#include "Features/DiskTerrainStreaming/StreamingRangeDebugRenderer.h"
#include "Features/DiskTerrainStreaming/StreamingRadiusPolicy.h"
#include "Features/QuadTreeLOD/QuadTreeLODSelector.h"
#include "Features/TriplanarTerrain/TriplanarMaterial.h"
#include "Graphics/ConstantBuffer.h"
#include "Graphics/Frustum.h"
#include "Graphics/Mesh.h"
#include "Graphics/Shader.h"
#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
struct DiskTerrainSettings
{
    QuadTreeLODSettings LOD={};
    // UI 슬라이더가 제어하는 고정 반경. 휠 Zoom과 독립이다.
    float LoadRadius=56.f,UnloadRadius=84.f;
    StreamingRadiusPolicy::Settings RadiusLimits={};
    std::uint32_t MaxPending=12u,MaxGpuUploadsPerFrame=2u;
    bool Streaming=true,Culling=true,LODEnabled=true;
    bool Prefetch=true,Cache=true;
    float PrefetchLead=48.f; // Camera 이동 방향 최대 선행 World Units.
    float PrefetchLookaheadSeconds=0.9f;
    std::uint32_t MaxPrefetchTargets=12u;
    std::size_t CacheBudgetMiB=1u; // CPU Halo samples만 보관, GPU Tile은 별도 관리.

};
struct DiskTerrainStats
{
    std::uint32_t TotalTiles=0,DesiredTiles=0,PendingTiles=0,LoadedTiles=0;
    std::uint32_t VisibleTiles=0,CulledTiles=0,FailedTiles=0,DrawCalls=0;
    std::uint64_t SurfaceTriangles=0,FullWorldTriangles=0,ResidentGpuMeshBytes=0;
    std::uint64_t DiskReads=0,DiskBytes=0;
    std::uint64_t CacheHits=0,CacheMisses=0;
    std::uint64_t PrefetchReads=0,PrefetchFailures=0,CacheEvictions=0;
    // Disk: 성공한 ReadTile 1회. Tile: 필수 Request 등록부터 GPU Mesh 설치 완료까지.
    StreamingTiming DiskReadTime,TileLoadTime;
    std::uint64_t CachedSampleBytes=0;
    std::uint32_t CachedTiles=0,PrefetchQueued=0;

    // CPU에 상주하는 것은 원본 HeightMap 전체가 아닌 제한된 LRU Tile Samples.
    // 호환용 필드: CachedSampleBytes와 같은 값이며 임시 CPU Mesh는 포함 안 함.
    std::uint64_t PersistentTileSampleBytes=0;
};
class DiskTerrainManager
{
public:
    DiskTerrainManager()=default;
    ~DiskTerrainManager();
    DiskTerrainManager(const DiskTerrainManager&)=delete;
    DiskTerrainManager& operator=(const DiskTerrainManager&)=delete;
    bool Initialize(ID3D11Device* device,ID3D11DeviceContext* context,
                    const std::filesystem::path& tileDirectory,
                    const std::filesystem::path& shaderDirectory,
                    const TriplanarMaterialDesc& material,
                    const DiskTerrainSettings& settings);
    // 카메라 Zoom은 전달하지 않는다: UI 반경으로만 로딩을 판단한다.
    void Update(const DirectX::XMFLOAT3& cameraTarget,float deltaSeconds=0.f);
    void SetPrefetch(bool value);
    void SetCache(bool value);
    void SetPrefetchLead(float value);
    void SetCacheBudgetMiB(float value);
    bool IsPrefetch()const;
    bool IsCache()const;
    float GetPrefetchLead()const;
    float GetCacheBudgetMiB()const;

    void Render(ID3D11DeviceContext* context,const DirectX::XMMATRIX& view,
                const DirectX::XMMATRIX& projection,
                const DirectX::XMFLOAT3& cameraPosition,bool showMiniMap);
    void SetStreaming(bool value);void SetCulling(bool value);void SetLOD(bool value);
    void SetBorders(bool value);
    void SetLoadRadius(float requestedRadius); // 유효성/최대값은 Manager가 보장
    void SetMiniMapZoomPercent(float value); // 미니맵 표기 전용, Tile 로딩과 독립
    float GetMiniMapZoomPercent() const;
    float GetMinimumMiniMapZoomPercent() const;
    float GetMaximumMiniMapZoomPercent() const;
    bool IsStreaming()const;bool IsCulling()const;bool IsLOD()const;bool IsBorders()const;
    float GetActiveLoadRadius()const;
    float GetActiveUnloadRadius()const;
    float GetMaximumLoadRadius()const;
    float GetMinimumLoadRadius()const;
    const DiskTerrainStats& GetStats()const;
    const std::wstring& GetError()const;
    const TileWorldInfo& GetWorld()const;
private:
    struct Resident
    {
        QuadTreeLOD Tree;
        Mesh TerrainMesh,BorderMesh;
        std::vector<QuadTreeLODDrawRange> Ranges;
        QuadTreeLODStats Frame;
        std::uint32_t FullStart=0,FullIndices=0,FullTriangles=0;
        std::uint64_t Bytes=0;
        bool Visible=false;
    };
    struct Slot
    {
        DirectX::BoundingBox Bounds;
        std::unique_ptr<Resident> Gpu;
        std::uint64_t Generation=0;
        // 해당 Generation의 필수 요청을 발행한 시각. 취소 결과는 계측 제외.
        std::chrono::steady_clock::time_point RequestStartedAt={};
        bool Desired=false,Pending=false,Failed=false;
    };
    bool Install(TilePrepared& data,Slot& slot);
    bool CreateStates(ID3D11Device* device);
    void UpdateStats();
    static float DistanceXZ(const DirectX::XMFLOAT3& focus,
                            const DirectX::BoundingBox& bounds);
    TileWorldInfo world_;
    std::vector<Slot> slots_;
    DiskTileWorker worker_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    TriplanarMaterial material_;
    StreamingRangeDebugRenderer miniMap_;
    Shader borderShader_;
    struct TransformCB {DirectX::XMFLOAT4X4 WVP,World;};
    ConstantBuffer<TransformCB> transforms_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> solidRS_,lineRS_;
    Frustum frustum_;
    DiskTerrainSettings settings_;

    // UI 슬라이더와 Tile 판정/MiniMap이 사용하는 단일 반경 상태.
    StreamingRadiusPolicy::Radii activeRadii_={56.0f,84.0f};

    DiskTerrainStats stats_;
    // Stats는 매 프레임 초기화되므로 누적 계측은 별도 멤버에 보관한다.
    StreamingTiming tileLoadTiming_;
    bool initialized_=false,showBorders_=false;
    DirectX::XMFLOAT3 focus_={0.f,0.f,0.f};
    DirectX::XMFLOAT3 previousFocus_={0.f,0.f,0.f};
    DirectX::XMFLOAT3 smoothedVelocity_={0.f,0.f,0.f};
    bool hasPreviousFocus_=false;

    std::wstring error_;
};
