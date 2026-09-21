// ============================================================================
// DiskTerrainManager.cpp — GPU 생명주기는 메인 스레드에서만 관리한다.
// ============================================================================
#include "Features/DiskTerrainStreaming/DiskTerrainManager.h"
#include "Features/DiskTerrainStreaming/MiniMapZoomPolicy.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
using namespace DirectX;
DiskTerrainManager::~DiskTerrainManager()
{
    worker_.Stop(); // Worker를 먼저 Join하여 메인 스레드의 객체 해제와 충돌 방지.
    slots_.clear();
}
bool DiskTerrainManager::Initialize(ID3D11Device* device,ID3D11DeviceContext* context,
    const std::filesystem::path& directory,const std::filesystem::path& shaders,
    const TriplanarMaterialDesc& material,const DiskTerrainSettings& settings)
{
    initialized_=false;worker_.Stop();slots_.clear();stats_={};error_.clear();
    if(!device || !context || !(settings.LoadRadius>0.f) ||
       !(settings.UnloadRadius>settings.LoadRadius) ||
       settings.MaxPending==0u || settings.MaxGpuUploadsPerFrame==0u ||
       !StreamingRadiusPolicy::IsValid(settings.RadiusLimits))
    {error_=L"Disk Terrain 설정 오류";return false;}
    settings_=settings;device_=device;context_=context;
    activeRadii_=StreamingRadiusPolicy::Calculate(settings.LoadRadius,settings.RadiusLimits);
    settings_.LoadRadius=activeRadii_.Load;
    settings_.UnloadRadius=activeRadii_.Unload;
    // 초기화 시 전체 HeightMap 로드 금지: 작은 Metadata 파일만 읽는다.
    if(!TileArchive::ReadMetadata(directory/L"Terrain.meta",world_,error_))return false;
    slots_.resize(world_.Tiles.size());
    float minimum=std::numeric_limits<float>::max();
    float maximum=-std::numeric_limits<float>::max();
    for(std::uint32_t z=0;z<world_.TilesZ;++z)
    for(std::uint32_t x=0;x<world_.TilesX;++x)
    {
        const auto index=z*world_.TilesX+x;
        const auto& tile=world_.Tiles[index];
        auto& slot=slots_[index];
        const auto sx=x*world_.CellsPerTile,sz=z*world_.CellsPerTile;
        const auto cx=std::min(world_.CellsPerTile,world_.TotalCellsX-sx);
        const auto cz=std::min(world_.CellsPerTile,world_.TotalCellsZ-sz);
        const float x0=world_.OriginX+sx*world_.CellSize;
        const float z0=world_.OriginZ+sz*world_.CellSize;
        const float x1=x0+cx*world_.CellSize;
        const float z1=z0+cz*world_.CellSize;
        const float lo=tile.MinimumHeight-settings.LOD.SkirtDepth;
        const float hi=tile.MaximumHeight;
        slot.Bounds.Center={(x0+x1)*0.5f,(lo+hi)*0.5f,(z0+z1)*0.5f};
        slot.Bounds.Extents={(x1-x0)*0.5f,std::max(0.001f,(hi-lo)*0.5f),(z1-z0)*0.5f};
        minimum=std::min(minimum,lo);maximum=std::max(maximum,hi);
    }
    if(!material_.Initialize(device,context,shaders,material))
    {error_=L"Triplanar Material: "+material_.GetLastErrorMessage();return false;}
    if(!borderShader_.Initialize(device,shaders.parent_path()/L"Basic/BasicVS.hlsl",
                                     shaders.parent_path()/L"Basic/BasicPS.hlsl"))
    {error_=L"Border Shader: "+borderShader_.GetLastErrorMessage();return false;}
    if(!transforms_.Initialize(device) || !CreateStates(device))
    {error_=L"Disk Terrain GPU Shader State 초기화 실패";return false;}
    if(!miniMap_.Initialize(device,shaders.parent_path()/L"Basic",
                            settings.LoadRadius,settings.UnloadRadius,minimum,maximum))
    {error_=L"MiniMap 초기화: "+miniMap_.GetLastErrorMessage();return false;}
    if(!worker_.Start(directory,world_,settings.LOD))
    {error_=L"Disk I/O Worker 생성 실패";return false;}
    stats_.TotalTiles=static_cast<std::uint32_t>(slots_.size());
    stats_.FullWorldTriangles=static_cast<std::uint64_t>(world_.TotalCellsX)*world_.TotalCellsZ*2ull;
    initialized_=true;return true;
}
float DiskTerrainManager::DistanceXZ(const XMFLOAT3& focus,const BoundingBox& bounds)
{
    // 화면상 원이 Tile의 중앙에 들어와야 하는 것이 아니라 AABB의 가장 가까운
    // 점까지의 XZ 거리로 판정한다. 큰 Tile이 원에 조금 걸쳐도 Load 가능하다.
    const float dx=std::max(std::abs(focus.x-bounds.Center.x)-bounds.Extents.x,0.f);
    const float dz=std::max(std::abs(focus.z-bounds.Center.z)-bounds.Extents.z,0.f);
    return std::sqrt(dx*dx+dz*dz);
}
void DiskTerrainManager::Update(const XMFLOAT3& target)
{
    if(!initialized_)return;
    focus_=target;

    // 휠 Zoom과 관계없이 UI에서 설정한 반경만 사용한다.
    activeRadii_=StreamingRadiusPolicy::Calculate(
        settings_.LoadRadius,settings_.RadiusLimits);

    // 실제 Tile 판정과 Mini Map 원을 같은 반경으로 동기화한다.
    miniMap_.SetRadii(activeRadii_.Load,activeRadii_.Unload);

    struct Candidate {std::uint32_t Index;float Distance;};
    std::vector<Candidate> candidates;
    std::uint32_t pending=0u;
    bool unbound=false;
    for(std::uint32_t i=0u;i<slots_.size();++i)
    {
        auto& slot=slots_[i];
        const float distance=DistanceXZ(target,slot.Bounds);
        const bool retained=static_cast<bool>(slot.Gpu) || slot.Pending;
        const bool keep=!settings_.Streaming ||
            distance<=(retained ? activeRadii_.Unload:activeRadii_.Load);
        if(slot.Desired!=keep)
        {
            slot.Desired=keep;
            ++slot.Generation;
            if(!keep)
            {
                slot.Failed=false;
                if(slot.Pending) {slot.Pending=false;worker_.Cancel(i,slot.Generation);}
            }
        }
        if(!keep)
        {
            // Immediate Context의 IA가 마지막 VB/IB를 보유할 수 있으므로 먼저 Unbind.
            if(slot.Gpu && !unbound)
            {
                ID3D11Buffer* nullBuffer=nullptr;
                const UINT zero=0u;
                context_->IASetVertexBuffers(0u,1u,&nullBuffer,&zero,&zero);
                context_->IASetIndexBuffer(nullptr,DXGI_FORMAT_UNKNOWN,0u);
                unbound=true;
            }
            slot.Gpu.reset();
        }
        else if(slot.Pending)++pending;
        else if(!slot.Gpu && !slot.Failed)candidates.push_back({i,distance});
    }
    std::sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b)
              {return a.Distance<b.Distance;});
    // 제한된 In-flight Job만 유지한다: 빠르게 카메라가 이동해도 긴 대기열 금지.
    for(const auto& candidate:candidates)
    {
        if(pending>=settings_.MaxPending)break;
        auto& slot=slots_[candidate.Index];
        slot.Pending=true;++pending;
        worker_.Request(candidate.Index,slot.Generation);
    }
    TilePrepared prepared;
    std::uint32_t attempts=0u,uploads=0u;
    while(attempts<settings_.MaxPending && uploads<settings_.MaxGpuUploadsPerFrame &&
          worker_.TryPop(prepared))
    {
        ++attempts;
        stats_.DiskReads+=(prepared.DiskBytes>0u?1ull:0ull);
        stats_.DiskBytes+=prepared.DiskBytes;
        if(prepared.Index>=slots_.size())continue;
        auto& slot=slots_[prepared.Index];
        // 현재 요청의 결과만 Pending을 해제한다. 오래된 결과가 새 요청을 무효화하지 않음.
        if(prepared.Generation!=slot.Generation || !slot.Desired)continue;
        slot.Pending=false;
        if(!prepared.Error.empty() || !Install(prepared,slot))
        {
            slot.Failed=true;
            error_=prepared.Error.empty()?L"Tile GPU Mesh 생성 실패":prepared.Error;
            continue;
        }
        error_.clear();++uploads;
    }
    UpdateStats();
}
bool DiskTerrainManager::Install(TilePrepared& prepared,Slot& slot)
{
    if(!prepared.Tree.GetRoot() || prepared.BorderVertices.empty())return false;
    auto resident=std::make_unique<Resident>();
    resident->Tree=std::move(prepared.Tree);
    resident->FullStart=prepared.MeshData.FullResolutionRange.StartIndex;
    resident->FullIndices=prepared.MeshData.FullResolutionRange.IndexCount;
    resident->FullTriangles=prepared.MeshData.FullResolutionRange.SurfaceTriangleCount;
    if(!resident->TerrainMesh.Initialize(device_.Get(),prepared.MeshData.Vertices,
                                        prepared.MeshData.Indices) ||
       !resident->BorderMesh.Initialize(device_.Get(),prepared.BorderVertices,
                                       prepared.BorderIndices))return false;
    resident->Bytes=static_cast<std::uint64_t>(prepared.MeshData.Vertices.size())*sizeof(Vertex)+
        static_cast<std::uint64_t>(prepared.MeshData.Indices.size())*sizeof(std::uint32_t)+
        static_cast<std::uint64_t>(prepared.BorderVertices.size())*sizeof(Vertex)+
        static_cast<std::uint64_t>(prepared.BorderIndices.size())*sizeof(std::uint32_t);
    resident->Ranges.reserve(resident->Tree.GetTotalLeafCount());
    slot.Gpu=std::move(resident);return true;
}
void DiskTerrainManager::UpdateStats()
{
    const auto diskReads=stats_.DiskReads,diskBytes=stats_.DiskBytes;
    stats_={};stats_.DiskReads=diskReads;stats_.DiskBytes=diskBytes;
    stats_.TotalTiles=static_cast<std::uint32_t>(slots_.size());
    stats_.FullWorldTriangles=static_cast<std::uint64_t>(world_.TotalCellsX)*world_.TotalCellsZ*2ull;
    for(const auto& slot:slots_)
    {
        if(slot.Desired)++stats_.DesiredTiles;
        if(slot.Pending)++stats_.PendingTiles;
        if(slot.Failed)++stats_.FailedTiles;
        if(slot.Gpu)
        {
            ++stats_.LoadedTiles;stats_.ResidentGpuMeshBytes+=slot.Gpu->Bytes;
        }
    }
    // CPU 원본 HeightMap은 없음. Worker의 일시 Sample/결과 버퍼는 포함하지 않음.
    stats_.PersistentTileSampleBytes=0;
}
void DiskTerrainManager::Render(ID3D11DeviceContext* context,const XMMATRIX& view,
    const XMMATRIX& projection,const XMFLOAT3& cameraPosition,bool showMiniMap)
{
    if(!initialized_ || !context)return;
    if(settings_.Culling)frustum_.Build(view,projection);
    TransformCB cb={};
    XMStoreFloat4x4(&cb.WVP,XMMatrixTranspose(view*projection));
    XMStoreFloat4x4(&cb.World,XMMatrixIdentity());
    transforms_.Update(context,cb);
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> previousRS;
    context->RSGetState(previousRS.GetAddressOf());
    context->RSSetState(solidRS_.Get());
    material_.Bind(context);transforms_.BindVS(context,0u);
    stats_.VisibleTiles=stats_.CulledTiles=stats_.DrawCalls=0u;
    stats_.SurfaceTriangles=0u;
    for(auto& slot:slots_)
    {
        if(!slot.Gpu)continue;
        auto& gpu=*slot.Gpu;gpu.Visible=false;
        if(settings_.Culling && frustum_.Contains(slot.Bounds)==DISJOINT)
        {++stats_.CulledTiles;continue;}
        gpu.Ranges.clear();gpu.Frame={};
        if(!settings_.Culling && !settings_.LODEnabled)
        {
            gpu.Ranges.push_back({gpu.FullStart,gpu.FullIndices,gpu.FullTriangles,0u});
            gpu.Frame.RenderedSurfaceTriangles=gpu.FullTriangles;
        }
        else
        {
            QuadTreeLODSelector::CollectDrawRanges(gpu.Tree,frustum_,XMMatrixIdentity(),
                cameraPosition,settings_.LOD,settings_.Culling,settings_.LODEnabled,
                gpu.FullTriangles,gpu.Ranges,gpu.Frame);
        }
        if(gpu.Ranges.empty()){++stats_.CulledTiles;continue;}
        gpu.Visible=true;++stats_.VisibleTiles;
        stats_.SurfaceTriangles+=gpu.Frame.RenderedSurfaceTriangles;
        gpu.TerrainMesh.Bind(context);
        for(const auto& range:gpu.Ranges)
        {gpu.TerrainMesh.DrawRange(context,range.IndexCount,range.StartIndex);++stats_.DrawCalls;}
    }
    material_.Unbind(context);
    if(showBorders_)
    {
        borderShader_.Bind(context);transforms_.BindVS(context,0u);
        context->RSSetState(lineRS_.Get());
        for(const auto& slot:slots_)
        {
            if(!slot.Gpu || !slot.Gpu->Visible)continue;
            slot.Gpu->BorderMesh.Bind(context);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
            slot.Gpu->BorderMesh.Draw(context);
        }
    }
    context->RSSetState(previousRS.Get());
    if(showMiniMap)
    {
        std::vector<StreamingRangeChunkInfo> display;
        display.reserve(slots_.size());
        for(const auto& slot:slots_)
        {
            StreamingRangeChunkInfo item;
            item.WorldBounds=slot.Bounds;
            item.Loaded=static_cast<bool>(slot.Gpu);
            item.Pending=slot.Pending;item.Desired=slot.Desired;item.Failed=slot.Failed;
            display.push_back(item);
        }
        miniMap_.SetFocus(focus_,XMMatrixIdentity());
        miniMap_.SetChunks(display);
        miniMap_.Render(context,view,projection,XMMatrixIdentity());
    }
}
bool DiskTerrainManager::CreateStates(ID3D11Device* device)
{
    D3D11_RASTERIZER_DESC solid={};
    solid.FillMode=D3D11_FILL_SOLID;solid.CullMode=D3D11_CULL_BACK;
    solid.DepthClipEnable=TRUE;
    if(FAILED(device->CreateRasterizerState(&solid,solidRS_.GetAddressOf())))return false;
    D3D11_RASTERIZER_DESC line={};
    line.FillMode=D3D11_FILL_SOLID;line.CullMode=D3D11_CULL_NONE;
    line.DepthClipEnable=TRUE;line.AntialiasedLineEnable=TRUE;
    line.DepthBias=-256;line.SlopeScaledDepthBias=-0.25f;
    return SUCCEEDED(device->CreateRasterizerState(&line,lineRS_.GetAddressOf()));
}
void DiskTerrainManager::SetStreaming(bool v){settings_.Streaming=v;}
void DiskTerrainManager::SetCulling(bool v){settings_.Culling=v;}
void DiskTerrainManager::SetLOD(bool v){settings_.LODEnabled=v;}
void DiskTerrainManager::SetBorders(bool v){showBorders_=v;}
void DiskTerrainManager::SetLoadRadius(float requested)
{
    // Manager가 모든 외부 UI 입력을 clamp한다. UI가 사라져도 안전하다.
    if(!std::isfinite(requested))return;
    const auto radii=StreamingRadiusPolicy::Calculate(requested,settings_.RadiusLimits);
    settings_.LoadRadius=radii.Load;
    settings_.UnloadRadius=radii.Unload;
    activeRadii_=radii;
    miniMap_.SetRadii(radii.Load,radii.Unload);
}
void DiskTerrainManager::SetMiniMapZoomPercent(float value)
{
    miniMap_.SetZoomPercent(value);
}
float DiskTerrainManager::GetMiniMapZoomPercent()const
{
    return miniMap_.GetZoomPercent();
}
float DiskTerrainManager::GetMinimumMiniMapZoomPercent()const
{
    return MiniMapZoomPolicy::MinimumPercent;
}
float DiskTerrainManager::GetMaximumMiniMapZoomPercent()const
{
    return MiniMapZoomPolicy::MaximumPercent;
}
bool DiskTerrainManager::IsStreaming()const{return settings_.Streaming;}
bool DiskTerrainManager::IsCulling()const{return settings_.Culling;}
bool DiskTerrainManager::IsLOD()const{return settings_.LODEnabled;}
bool DiskTerrainManager::IsBorders()const{return showBorders_;}
float DiskTerrainManager::GetActiveLoadRadius()const{return activeRadii_.Load;}
float DiskTerrainManager::GetActiveUnloadRadius()const{return activeRadii_.Unload;}
float DiskTerrainManager::GetMaximumLoadRadius()const{return settings_.RadiusLimits.MaximumLoad;}
float DiskTerrainManager::GetMinimumLoadRadius()const{return settings_.RadiusLimits.MinimumLoad;}
const DiskTerrainStats& DiskTerrainManager::GetStats()const{return stats_;}
const std::wstring& DiskTerrainManager::GetError()const{return error_;}
const TileWorldInfo& DiskTerrainManager::GetWorld()const{return world_;}
