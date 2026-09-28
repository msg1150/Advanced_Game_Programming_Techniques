// ============================================================================
// DiskTerrainManager.cpp — GPU 생명주기는 메인 스레드에서만 관리한다.
// ============================================================================
#include "Features/DiskTerrainStreaming/DiskTerrainManager.h"
#include "Features/DiskTerrainStreaming/MiniMapZoomPolicy.h"
#include "Features/DiskTerrainStreaming/PrefetchPolicy.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <utility>
using namespace DirectX;
DiskTerrainManager::~DiskTerrainManager()
{
    // 두 CPU Worker를 모두 Join한 뒤 GPU Resident를 해제한다.
    // Geomorph Worker는 Disk Worker와 GPU 객체를 공유하지 않으므로 종료 순서 의존성은 없다.
    worker_.Stop();
    geomorphWorker_.Stop();
    slots_.clear();
}
bool DiskTerrainManager::Initialize(ID3D11Device* device,ID3D11DeviceContext* context,
    const std::filesystem::path& directory,const std::filesystem::path& shaders,
    const TriplanarMaterialDesc& material,const DiskTerrainSettings& settings)
{
    initialized_=false;worker_.Stop();geomorphWorker_.Stop();slots_.clear();stats_={};
    tileLoadTiming_={};error_.clear();
    if(!device || !context || !(settings.LoadRadius>0.f) ||
       !(settings.UnloadRadius>settings.LoadRadius) ||
       settings.MaxPending==0u || settings.MaxGpuUploadsPerFrame==0u ||
       !StreamingRadiusPolicy::IsValid(settings.RadiusLimits) ||
       !QuadTreeLODGeomorphPolicy::IsValid(settings.Geomorph))
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

    // Geomorph는 기존 Triplanar Material의 Texture/Sampler/PS 설정을 재사용하고
    // Vertex Shader만 별도 Feature Shader로 교체한다.
    if(!geomorphRenderer_.Initialize(device,shaders))
    {error_=L"QuadTree Geomorph: "+geomorphRenderer_.GetLastErrorMessage();return false;}

    if(!borderShader_.Initialize(device,shaders.parent_path()/L"Basic/BasicVS.hlsl",
                                     shaders.parent_path()/L"Basic/BasicPS.hlsl"))
    {error_=L"Border Shader: "+borderShader_.GetLastErrorMessage();return false;}
    if(!transforms_.Initialize(device) || !CreateStates(device))
    {error_=L"Disk Terrain GPU Shader State 초기화 실패";return false;}
    if(!miniMap_.Initialize(device,shaders.parent_path()/L"Basic",
                            settings.LoadRadius,settings.UnloadRadius,minimum,maximum))
    {error_=L"MiniMap 초기화: "+miniMap_.GetLastErrorMessage();return false;}
    // 기본 Disk Worker는 기존 v0.9/v0.10 구조 그대로 유지한다.
    if(!worker_.Start(directory,world_,settings.LOD))
    {error_=L"Disk I/O Worker 생성 실패";return false;}

    // v0.10에서 Main Thread Install()이 하던 Geomorph Mesh 재구성을
    // 별도 CPU Worker로 분리한다. 이 Worker는 Direct3D API를 호출하지 않는다.
    if(!geomorphWorker_.Start(world_,settings.LOD))
    {
        worker_.Stop();
        error_=L"QuadTree Geomorph CPU Worker 생성 실패";
        return false;
    }

    worker_.SetCacheCapacity(settings_.Cache ? settings_.CacheBudgetMiB*1024u*1024u : 0u);
    hasPreviousFocus_=false;smoothedVelocity_={0.f,0.f,0.f};
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
void DiskTerrainManager::Update(const XMFLOAT3& target,float deltaSeconds)
{
    if(!initialized_)return;
    focus_=target;
    // 카메라 Target XZ 변화만 사용. 마우스 휠/카메라 회전은 예측에 영향 없음.
    if(!hasPreviousFocus_)
    {
        previousFocus_=target;hasPreviousFocus_=true;
        smoothedVelocity_={0.f,0.f,0.f};
    }
    else if(deltaSeconds>0.0001f && deltaSeconds<=0.2f)
    {
        const float instantX=(target.x-previousFocus_.x)/deltaSeconds;
        const float instantZ=(target.z-previousFocus_.z)/deltaSeconds;
        const float alpha=std::clamp(deltaSeconds*12.f,0.f,1.f);
        smoothedVelocity_.x+=(instantX-smoothedVelocity_.x)*alpha;
        smoothedVelocity_.z+=(instantZ-smoothedVelocity_.z)*alpha;
        previousFocus_=target;
    }
    else
    {
        // 긴 프레임/초기화/Alt+Tab 이후 잘못된 속도로 먼 Tile을 요청하지 않는다.
        previousFocus_=target;
        smoothedVelocity_={0.f,0.f,0.f};
    }

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
                if(slot.Pending)
                {
                    slot.Pending=false;
                    worker_.Cancel(i,slot.Generation);
                    geomorphWorker_.Cancel(i,slot.Generation);
                }
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
    // GPU에 실재하는 Tile은 여전히 기존 Load/Unload 반경만 따른다.
    // Prefetch는 오직 별도 CPU 높이 Sample Cache를 미리 채운다.
    std::vector<std::uint32_t> prefetchTargets;
    if(settings_.Streaming && settings_.Prefetch && settings_.Cache &&
       settings_.PrefetchLead>0.f && settings_.CacheBudgetMiB>0u)
    {
        const auto offset=PrefetchPolicy::PredictOffset(
            smoothedVelocity_.x,smoothedVelocity_.z,
            settings_.PrefetchLookaheadSeconds,settings_.PrefetchLead);
        if(offset.X!=0.f || offset.Z!=0.f)
        {
            XMFLOAT3 predicted=target;
            predicted.x+=offset.X;
            predicted.z+=offset.Z;
            std::vector<Candidate> speculative;
            for(std::uint32_t i=0;i<slots_.size();++i)
            {
                const auto& slot=slots_[i];
                if(slot.Desired || slot.Pending || slot.Gpu || slot.Failed)continue;
                const float predictedDistance=DistanceXZ(predicted,slot.Bounds);
                if(predictedDistance<=activeRadii_.Load)
                    speculative.push_back({i,predictedDistance});
            }
            std::sort(speculative.begin(),speculative.end(),
                [](const Candidate&a,const Candidate&b){return a.Distance<b.Distance;});
            for(const auto& candidate:speculative)
            {
                if(prefetchTargets.size()>=settings_.MaxPrefetchTargets)break;
                prefetchTargets.push_back(candidate.Index);
            }
        }
    }
    std::sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b)
              {return a.Distance<b.Distance;});
    // 제한된 In-flight Job만 유지한다: 빠르게 카메라가 이동해도 긴 대기열 금지.
    for(const auto& candidate:candidates)
    {
        if(pending>=settings_.MaxPending)break;
        auto& slot=slots_[candidate.Index];
        slot.Pending=true;++pending;
        slot.RequestStartedAt=std::chrono::steady_clock::now();
        worker_.Request(candidate.Index,slot.Generation);
    }
    // 필수 Job들을 먼저 큐에 쌓은 후 예측 작업을 교체한다.
    // 디스크 스레드는 필수 Job이 있는 동안 Prefetch를 시작하지 않는다.
    worker_.SetPrefetchTargets(prefetchTargets);
    // ------------------------------------------------------------------------
    // 1단계: Disk Worker 결과 -> Geomorph CPU Worker
    // ------------------------------------------------------------------------
    // v0.10에서는 이 지점 이후 Main Thread가 즉시 Geomorph Mesh를 생성했기 때문에
    // 새 Tile이 들어오는 프레임에 큰 CPU Spike가 발생할 수 있었다.
    // 이제 Main Thread는 준비된 Tile을 전용 Worker로 Move만 하고 바로 반환한다.
    TilePrepared diskPrepared;
    std::uint32_t diskResults=0u;

    while(diskResults<settings_.MaxPending && worker_.TryPop(diskPrepared))
    {
        ++diskResults;

        if(diskPrepared.Index>=slots_.size())continue;
        auto& slot=slots_[diskPrepared.Index];

        // 오래된 Generation은 Disk Worker와 동일한 규칙으로 폐기한다.
        if(diskPrepared.Generation!=slot.Generation || !slot.Desired)continue;

        if(!diskPrepared.Error.empty())
        {
            slot.Pending=false;
            slot.Failed=true;
            error_=diskPrepared.Error;
            continue;
        }

        // Pending은 Geomorph CPU 준비 + GPU Install이 끝날 때까지 유지한다.
        // 따라서 기존 MaxPending 제한이 그대로 두 Worker 전체 파이프라인을 제한한다.
        if(!geomorphWorker_.Submit(diskPrepared))
        {
            slot.Pending=false;
            slot.Failed=true;
            error_=L"Geomorph CPU Worker 요청 실패";
        }
    }

    // ------------------------------------------------------------------------
    // 2단계: Geomorph CPU Worker 결과 -> Main Thread GPU Upload
    // ------------------------------------------------------------------------
    // GPU Resource 생성은 D3D11 Immediate Context 안전성을 위해 기존대로 Main Thread만 수행한다.
    QuadTreeLODGeomorphPreparedTile prepared;
    std::uint32_t attempts=0u,uploads=0u;

    while(attempts<settings_.MaxPending &&
          uploads<settings_.MaxGpuUploadsPerFrame &&
          geomorphWorker_.TryPop(prepared))
    {
        ++attempts;

        const auto index=prepared.Tile.Index;
        if(index>=slots_.size())continue;
        auto& slot=slots_[index];

        if(prepared.Tile.Generation!=slot.Generation || !slot.Desired)continue;

        slot.Pending=false;

        if(!prepared.Tile.Error.empty() || !Install(prepared,slot))
        {
            slot.Failed=true;
            error_=prepared.Tile.Error.empty()?L"Tile GPU Mesh 생성 실패":prepared.Tile.Error;
            continue;
        }

        // Disk/Cache + 기본 CPU LOD + Background Geomorph + Main Thread GPU Upload까지 포함한다.
        // GPU에서 Draw가 실제 완료되는 시간은 기존과 마찬가지로 포함하지 않는다.
        tileLoadTiming_.Record(std::chrono::steady_clock::now()-slot.RequestStartedAt);
        error_.clear();
        ++uploads;
    }
    UpdateStats();
}
bool DiskTerrainManager::Install(
    QuadTreeLODGeomorphPreparedTile& prepared,
    Slot& slot)
{
    TilePrepared& tile=prepared.Tile;

    if(!tile.Tree.GetRoot() || tile.BorderVertices.empty())return false;

    auto resident=std::make_unique<Resident>();
    resident->Tree=std::move(tile.Tree);
    resident->GeomorphAvailable=prepared.GeomorphAvailable;

    // ------------------------------------------------------------------------
    // Main Thread에서는 더 이상 GeomorphBuilder::Build를 호출하지 않는다.
    // 무거운 Parent Surface 계산 / Node Vertex 복제 / unordered_map 작업은
    // QuadTreeLODGeomorphWorker가 Background Thread에서 완료한 상태다.
    // 여기서는 완성 CPU 배열을 GPU Buffer로 올리는 일만 수행한다.
    // ------------------------------------------------------------------------
    if(resident->GeomorphAvailable)
    {
        resident->FullStart=prepared.GeomorphMesh.FullResolutionRange.StartIndex;
        resident->FullIndices=prepared.GeomorphMesh.FullResolutionRange.IndexCount;
        resident->FullTriangles=prepared.GeomorphMesh.FullResolutionRange.SurfaceTriangleCount;

        if(!resident->TerrainMesh.Initialize(
                device_.Get(),
                prepared.GeomorphMesh.Vertices,
                prepared.GeomorphMesh.Indices))
        {
            return false;
        }

        resident->Bytes=
            static_cast<std::uint64_t>(prepared.GeomorphMesh.Vertices.size())*sizeof(Vertex)+
            static_cast<std::uint64_t>(prepared.GeomorphMesh.Indices.size())*sizeof(std::uint32_t);

        // Runtime에는 Range Mapping만 필요하다.
        // v0.10처럼 Resident 안에서 대형 CPU Vertex/Index를 clear + shrink_to_fit 하지 않고,
        // Mapping만 Move하여 유지한다. 준비 객체의 Vertex/Index는 함수 종료 후 자연스럽게
        // 해제되므로 메인 Thread에서 불필요한 shrink_to_fit 재할당을 수행하지 않는다.
        resident->GeomorphRanges=std::move(prepared.GeomorphMesh.Ranges);
    }
    else
    {
        // Background Geomorph 생성이 실패해도 기존 Terrain Mesh로 자동 Fallback한다.
        // 기존 QuadTree LOD / Streaming 경로에는 영향을 주지 않는다.
        resident->FullStart=tile.MeshData.FullResolutionRange.StartIndex;
        resident->FullIndices=tile.MeshData.FullResolutionRange.IndexCount;
        resident->FullTriangles=tile.MeshData.FullResolutionRange.SurfaceTriangleCount;

        if(!resident->TerrainMesh.Initialize(
                device_.Get(),
                tile.MeshData.Vertices,
                tile.MeshData.Indices))
        {
            return false;
        }

        resident->Bytes=
            static_cast<std::uint64_t>(tile.MeshData.Vertices.size())*sizeof(Vertex)+
            static_cast<std::uint64_t>(tile.MeshData.Indices.size())*sizeof(std::uint32_t);
    }

    if(!resident->BorderMesh.Initialize(
            device_.Get(),
            tile.BorderVertices,
            tile.BorderIndices))
    {
        return false;
    }

    resident->Bytes+=
        static_cast<std::uint64_t>(tile.BorderVertices.size())*sizeof(Vertex)+
        static_cast<std::uint64_t>(tile.BorderIndices.size())*sizeof(std::uint32_t);

    resident->Ranges.reserve(resident->Tree.GetTotalLeafCount());
    slot.Gpu=std::move(resident);
    return true;
}

void DiskTerrainManager::UpdateStats()
{
    const auto metrics=worker_.GetMetrics();
    stats_={};
    stats_.DiskReads=metrics.DiskReads;stats_.DiskBytes=metrics.DiskBytes;
    stats_.CacheHits=metrics.CacheHits;
    stats_.CacheMisses=metrics.CacheMisses;
    stats_.DiskReadTime=metrics.DiskReadTime;
    stats_.TileLoadTime=tileLoadTiming_;
    stats_.PrefetchReads=metrics.PrefetchReads;
    stats_.PrefetchFailures=metrics.PrefetchFailures;
    stats_.CacheEvictions=metrics.CacheEvictions;
    stats_.CachedSampleBytes=metrics.CachedBytes;
    stats_.CachedTiles=metrics.CachedTiles;
    stats_.PrefetchQueued=metrics.PrefetchQueued;
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
    // 원본 HeightMap 전체는 없고 LRU 높이 샘플만 상주한다.
    // Worker 작업 중 일시 버퍼 및 OS 파일 캐시는 이 수치에 포함하지 않는다.
    stats_.PersistentTileSampleBytes=stats_.CachedSampleBytes;
}
void DiskTerrainManager::Render(ID3D11DeviceContext* context,const XMMATRIX& view,
    const XMMATRIX& projection,const XMFLOAT3& cameraPosition,bool showMiniMap)
{
    if(!initialized_ || !context)return;
    if(settings_.Culling)frustum_.Build(view,projection);

    const XMMATRIX world=XMMatrixIdentity();

    TransformCB cb={};
    XMStoreFloat4x4(&cb.WVP,XMMatrixTranspose(view*projection));
    XMStoreFloat4x4(&cb.World,world);
    transforms_.Update(context,cb);

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> previousRS;
    context->RSGetState(previousRS.GetAddressOf());
    context->RSSetState(solidRS_.Get());

    // 기존 Triplanar Material이 Texture/Sampler/Pixel Shader/설정 Buffer를 Bind한다.
    // Geomorphing이 필요한 Draw에서만 Vertex Shader를 별도 Shader로 교체한다.
    material_.Bind(context);
    transforms_.BindVS(context,0u);

    bool geomorphShaderBound=false;

    stats_.VisibleTiles=stats_.CulledTiles=stats_.DrawCalls=0u;
    stats_.SurfaceTriangles=0u;

    for(auto& slot:slots_)
    {
        if(!slot.Gpu)continue;

        auto& gpu=*slot.Gpu;
        gpu.Visible=false;

        if(settings_.Culling && frustum_.Contains(slot.Bounds)==DISJOINT)
        {
            ++stats_.CulledTiles;
            continue;
        }

        gpu.Ranges.clear();
        gpu.Frame={};

        if(!settings_.Culling && !settings_.LODEnabled)
        {
            gpu.Ranges.push_back({gpu.FullStart,gpu.FullIndices,gpu.FullTriangles,0u});
            gpu.Frame.RenderedSurfaceTriangles=gpu.FullTriangles;
        }
        else
        {
            QuadTreeLODSelector::CollectDrawRanges(
                gpu.Tree,frustum_,world,cameraPosition,
                settings_.LOD,settings_.Culling,settings_.LODEnabled,
                gpu.FullTriangles,gpu.Ranges,gpu.Frame);
        }

        if(gpu.Ranges.empty())
        {
            ++stats_.CulledTiles;
            continue;
        }

        gpu.Visible=true;
        ++stats_.VisibleTiles;
        stats_.SurfaceTriangles+=gpu.Frame.RenderedSurfaceTriangles;

        gpu.TerrainMesh.Bind(context);

        // Full Resolution 수동 경로는 StartIndex가 이미 실제 TerrainMesh 기준이다.
        const bool manualFullResolution=
            !settings_.Culling && !settings_.LODEnabled;

        for(const auto& range:gpu.Ranges)
        {
            std::uint32_t drawStartIndex=range.StartIndex;
            const QuadTreeLODGeomorphRangeBinding* binding=nullptr;

            // Geomorph Mesh를 GPU에 올린 Tile은 Tree의 기존 StartIndex를
            // 새 Mesh StartIndex로 변환해야 한다.
            // Geomorphing Checkbox가 OFF여도 이 Mapping은 필요하다.
            if(gpu.GeomorphAvailable && !manualFullResolution)
            {
                binding=QuadTreeLODGeomorphBuilder::FindRange(
                    gpu.GeomorphRanges,
                    range.StartIndex);

                // Mapping이 없으면 잘못된 범위를 그리지 않고 해당 Range만 건너뛴다.
                if(!binding)continue;

                drawStartIndex=binding->GeomorphStartIndex;
            }

            const bool useGeomorph=
                settings_.GeomorphEnabled &&
                settings_.LODEnabled &&
                gpu.GeomorphAvailable &&
                binding &&
                binding->HasParent;

            if(useGeomorph)
            {
                if(!geomorphShaderBound)
                {
                    // Debug Visualization은 Shader 선택만 바꾸며 Morph Factor 계산,
                    // LOD 선택, Mesh/Streaming 데이터에는 손대지 않는다.
                    geomorphRenderer_.Begin(
                        context,
                        settings_.GeomorphDebugVisualization);
                    transforms_.BindVS(context,0u);
                    geomorphShaderBound=true;
                }

                const float morphFactor=
                    QuadTreeLODGeomorphBuilder::CalculateMorphFactor(
                        *binding,
                        cameraPosition,
                        world,
                        settings_.LOD,
                        settings_.Geomorph);

                geomorphRenderer_.SetMorphFactor(context,morphFactor);
            }
            else
            {
                // 앞 Range에서 Geomorph Shader를 사용했다면 기존 Triplanar Shader로
                // 되돌린다. Texture/Sampler/PS 설정도 Material이 동일하게 복구한다.
                if(geomorphShaderBound)
                {
                    material_.Bind(context);
                    transforms_.BindVS(context,0u);
                    geomorphShaderBound=false;
                }
            }

            gpu.TerrainMesh.DrawRange(
                context,
                range.IndexCount,
                drawStartIndex);

            ++stats_.DrawCalls;
        }
    }

    // 마지막 Range가 Geomorph Shader였더라도 Material Unbind는 Texture/Sampler만
    // 정리하므로 그대로 호출해도 안전하다.
    material_.Unbind(context);

    if(showBorders_)
    {
        borderShader_.Bind(context);
        transforms_.BindVS(context,0u);
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
            item.Pending=slot.Pending;
            item.Desired=slot.Desired;
            item.Failed=slot.Failed;
            display.push_back(item);
        }

        miniMap_.SetFocus(focus_,world);
        miniMap_.SetChunks(display);
        miniMap_.Render(context,view,projection,world);
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
void DiskTerrainManager::SetPrefetch(bool value){settings_.Prefetch=value;}
void DiskTerrainManager::SetCache(bool value)
{
    settings_.Cache=value;
    worker_.SetCacheCapacity(value ? settings_.CacheBudgetMiB*1024u*1024u : 0u);
    if(!value)worker_.SetPrefetchTargets({});
}
void DiskTerrainManager::SetPrefetchLead(float value)
{
    if(std::isfinite(value))settings_.PrefetchLead=std::clamp(value,0.f,96.f);
}
void DiskTerrainManager::SetCacheBudgetMiB(float value)
{
    if(!std::isfinite(value))return;
    settings_.CacheBudgetMiB=static_cast<std::size_t>(std::clamp(std::round(value),1.f,16.f));
    if(settings_.Cache)
        worker_.SetCacheCapacity(settings_.CacheBudgetMiB*1024u*1024u);
}
bool DiskTerrainManager::IsPrefetch()const{return settings_.Prefetch;}
bool DiskTerrainManager::IsCache()const{return settings_.Cache;}
float DiskTerrainManager::GetPrefetchLead()const{return settings_.PrefetchLead;}
float DiskTerrainManager::GetCacheBudgetMiB()const{return static_cast<float>(settings_.CacheBudgetMiB);}
void DiskTerrainManager::SetStreaming(bool v){settings_.Streaming=v;}
void DiskTerrainManager::SetCulling(bool v){settings_.Culling=v;}
void DiskTerrainManager::SetLOD(bool v){settings_.LODEnabled=v;}
void DiskTerrainManager::SetGeomorph(bool v){settings_.GeomorphEnabled=v;}
void DiskTerrainManager::SetGeomorphDebugVisualization(bool v)
{
    settings_.GeomorphDebugVisualization=v;
}
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
bool DiskTerrainManager::IsGeomorph()const{return settings_.GeomorphEnabled;}
bool DiskTerrainManager::IsGeomorphDebugVisualization()const
{
    return settings_.GeomorphDebugVisualization;
}
bool DiskTerrainManager::IsBorders()const{return showBorders_;}
float DiskTerrainManager::GetActiveLoadRadius()const{return activeRadii_.Load;}
float DiskTerrainManager::GetActiveUnloadRadius()const{return activeRadii_.Unload;}
float DiskTerrainManager::GetMaximumLoadRadius()const{return settings_.RadiusLimits.MaximumLoad;}
float DiskTerrainManager::GetMinimumLoadRadius()const{return settings_.RadiusLimits.MinimumLoad;}
const DiskTerrainStats& DiskTerrainManager::GetStats()const{return stats_;}
const std::wstring& DiskTerrainManager::GetError()const{return error_;}
const TileWorldInfo& DiskTerrainManager::GetWorld()const{return world_;}
