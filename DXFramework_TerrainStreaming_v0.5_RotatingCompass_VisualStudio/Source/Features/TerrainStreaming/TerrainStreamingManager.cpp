// ============================================================================
// TerrainStreamingManager.cpp
// GPU 작업은 UpdateStreaming / Render가 호출되는 메인 스레드에서만 수행한다.
// ============================================================================
#include "Features/TerrainStreaming/TerrainStreamingManager.h"
#include "Features/TerrainStreaming/TerrainStreamingPolicy.h"
#include "Features/HeightMapTerrain/HeightMapImage.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <limits>
#include <utility>

using namespace DirectX;
TerrainStreamingManager::~TerrainStreamingManager()
{
    // GPU Slot보다 Worker를 먼저 정지하고 Join한다. 소멸 중 접근 위험 방지.
    worker_.Stop();
    slots_.clear();
}

bool TerrainStreamingManager::Initialize(ID3D11Device* device,ID3D11DeviceContext* context,
    const std::filesystem::path& heightmapPath,const HeightMapTerrainSettings& heights,
    const std::filesystem::path& shaderDir,const TriplanarMaterialDesc& mat,
    const TerrainStreamingSettings& settings)
{
    initialized_=false; worker_.Stop(); slots_.clear(); stats_={}; lastError_.clear();
    if(!device || !context || settings.CellsPerChunk==0u ||
       !(settings.LoadRadius>0.f) || !(settings.UnloadRadius>settings.LoadRadius) ||
       settings.MaxUploadsPerFrame==0u)
    { lastError_=L"Streaming 설정 오류: Radius/ChunkSize/UploadBudget를 확인하세요."; return false; }
    settings_=settings;
    device_=device;
    context_=context;
    HeightMapImage image;
    if(!image.Load(heightmapPath))
    { lastError_=L"Streaming HeightMap 로드 실패: "+image.GetLastErrorMessage(); return false; }
    HeightMapTerrainMeshData full;
    std::wstring error;
    if(!HeightMapTerrainGenerator::Generate(image,heights,full,error))
    { lastError_=L"Streaming HeightMap Geometry 실패: "+error; return false; }

    const auto width=image.GetWidth(),height=image.GetHeight();
    const auto cellsX=width-1u,cellsZ=height-1u;
    const auto chunksX=1u+(cellsX-1u)/settings.CellsPerChunk;
    const auto chunksZ=1u+(cellsZ-1u)/settings.CellsPerChunk;
    if(static_cast<std::uint64_t>(chunksX)*chunksZ>std::numeric_limits<std::uint32_t>::max())
    { lastError_=L"Streaming Chunk 수 범위 초과"; return false; }
    std::vector<StreamingChunkDescriptor> descriptors;
    descriptors.reserve(static_cast<std::size_t>(chunksX)*chunksZ);
    slots_.reserve(static_cast<std::size_t>(chunksX)*chunksZ);
    for(std::uint32_t z=0;z<chunksZ;++z)
    for(std::uint32_t x=0;x<chunksX;++x)
    {
        StreamingChunkDescriptor d;
        d.Index=static_cast<std::uint32_t>(descriptors.size());
        d.ChunkX=x;d.ChunkZ=z;
        d.StartX=x*settings.CellsPerChunk;d.StartZ=z*settings.CellsPerChunk;
        d.CellsX=std::min(settings.CellsPerChunk,cellsX-d.StartX);
        d.CellsZ=std::min(settings.CellsPerChunk,cellsZ-d.StartZ);
        d.LastX=x+1u==chunksX;d.LastZ=z+1u==chunksZ;
        XMFLOAT3 lo={FLT_MAX,FLT_MAX,FLT_MAX},hi={-FLT_MAX,-FLT_MAX,-FLT_MAX};
        for(std::uint32_t iz=d.StartZ;iz<=d.StartZ+d.CellsZ;++iz)
        for(std::uint32_t ix=d.StartX;ix<=d.StartX+d.CellsX;++ix)
        {
            const auto& v=full.Vertices[static_cast<std::size_t>(iz)*width+ix].Position;
            lo.x=std::min(lo.x,v.x);lo.y=std::min(lo.y,v.y);lo.z=std::min(lo.z,v.z);
            hi.x=std::max(hi.x,v.x);hi.y=std::max(hi.y,v.y);hi.z=std::max(hi.z,v.z);
        }
        lo.y-=settings.LOD.SkirtDepth;
        d.Bounds.Center={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f,(lo.z+hi.z)*.5f};
        d.Bounds.Extents={(hi.x-lo.x)*.5f,(hi.y-lo.y)*.5f,(hi.z-lo.z)*.5f};
        d.Bounds.Extents.y=std::max(d.Bounds.Extents.y,.001f);
        Slot slot;slot.Desc=d;slots_.push_back(std::move(slot));
        descriptors.push_back(d);
    }
    if(!material_.Initialize(device,context,shaderDir,mat))
    { lastError_=L"Streaming Material: "+material_.GetLastErrorMessage(); return false; }
    if(!borderShader_.Initialize(device,shaderDir.parent_path()/L"Basic"/L"BasicVS.hlsl",
                                      shaderDir.parent_path()/L"Basic"/L"BasicPS.hlsl"))
    { lastError_=L"Streaming Border Shader: "+borderShader_.GetLastErrorMessage(); return false; }
    if(!transforms_.Initialize(device)||!CreateRasterizers(device))
    { lastError_=L"Streaming Transform/Rasterizer GPU 초기화 실패";return false; }

    // HeightMap 전체 높이를 한 번만 계산하여 시야 옆에서도 확인 가능한
    // 두 개의 3D 원기둥 Debug Mesh를 만든다. Worker / Streaming Policy 불변.
    float minHeight=FLT_MAX;
    float maxHeight=-FLT_MAX;
    for(const Vertex& vertex:full.Vertices)
    {
        minHeight=std::min(minHeight,vertex.Position.y);
        maxHeight=std::max(maxHeight,vertex.Position.y);
    }
    if(!rangeDebugRenderer_.Initialize(
        device,shaderDir.parent_path()/L"Basic",
        settings.LoadRadius,settings.UnloadRadius,minHeight,maxHeight))
    {
        lastError_=L"Streaming Range Debug 초기화 실패: " +
                   rangeDebugRenderer_.GetLastErrorMessage();
        return false;
    }

    if(!worker_.Start(std::move(full.Vertices),width,height,settings.LOD,std::move(descriptors)))
    { lastError_=L"Streaming CPU Worker 시작 실패";return false; }
    stats_.TotalChunks=static_cast<std::uint32_t>(slots_.size());
    stats_.FullResolutionSurfaceTriangles=cellsX*cellsZ*2u;
    initialized_=true;
    return true;
}

float TerrainStreamingManager::DistanceXZ(const XMFLOAT3& focus,const BoundingBox& bounds)
{
    const float dx=std::max(std::abs(focus.x-bounds.Center.x)-bounds.Extents.x,0.f);
    const float dz=std::max(std::abs(focus.z-bounds.Center.z)-bounds.Extents.z,0.f);
    return std::sqrt(dx*dx+dz*dz);
}

void TerrainStreamingManager::UpdateStreaming(const XMFLOAT3& focus)
{
    if(!initialized_) return;
    const XMMATRIX world=transform_.GetWorldMatrix();
    struct Candidate{std::uint32_t Index;float Distance;};
    std::vector<Candidate> toRequest;
    bool inputAssemblerUnbound=false;
    for(auto& slot:slots_)
    {
        BoundingBox worldBounds;
        slot.Desc.Bounds.Transform(worldBounds,world);
        const float distance=DistanceXZ(focus,worldBounds);
        // 히스테리시스: 이미 GPU에 있거나 요청된 Chunk는 UnloadRadius까지 유지.
        const bool retained=slot.Gpu!=nullptr || slot.Pending;
        const bool desired=TerrainStreamingPolicy::ShouldKeep(
            settings_.EnableStreaming,retained,distance,
            settings_.LoadRadius,settings_.UnloadRadius);
        if(slot.Desired!=desired)
        { slot.Desired=desired; ++slot.Generation; if(!desired) slot.Failed=false; }
        if(!desired)
        {
            // Immediate Context가 마지막 Mesh VB/IB 참조를 보유할 수 있으므로
            // Frame 시작 시 먼저 IA 참조를 끊는다. GPU 작업은 이 스레드 전용.
            if(slot.Gpu && !inputAssemblerUnbound && context_)
            {
                ID3D11Buffer* nullBuffer=nullptr;
                const UINT zero=0u;
                context_->IASetVertexBuffers(0u,1u,&nullBuffer,&zero,&zero);
                context_->IASetIndexBuffer(nullptr,DXGI_FORMAT_UNKNOWN,0u);
                inputAssemblerUnbound=true;
            }
            slot.Gpu.reset();
        }
        else if(!slot.Gpu && !slot.Pending && !slot.Failed)
            toRequest.push_back({slot.Desc.Index,distance});
    }
    // 가까운 Chunk부터 요청한다. 카메라가 이동하면 다음 프레임에 우선순위 재평가.
    std::sort(toRequest.begin(),toRequest.end(),
        [](const Candidate&a,const Candidate&b){return a.Distance<b.Distance;});
    for(const auto& c:toRequest)
    {
        Slot& slot=slots_[c.Index];
        slot.Pending=true;
        worker_.Request(c.Index,slot.Generation);
    }

    // 한 프레임의 GPU 업로드 횟수를 제한하여 긴 프레임을 줄인다.
    std::uint32_t uploads=0u;
    StreamingCPUResult result;
    while(uploads<settings_.MaxUploadsPerFrame && worker_.TryPop(result))
    {
        if(result.Index>=slots_.size()) continue;
        Slot& slot=slots_[result.Index];
        slot.Pending=false;
        // 카메라가 이동해 취소된 오래된 결과를 버린다. GPU 업로드 금지.
        if(!slot.Desired || result.Generation!=slot.Generation) continue;
        if(!result.Error.empty() || !Install(result,slot))
        {
            slot.Failed=true;
            lastError_=result.Error.empty()?L"Chunk GPU Mesh/Border 생성 실패":result.Error;
            continue;
        }
        lastError_.clear();
        ++uploads;
    }

    // 디버그 UI에는 실제 Streaming Policy가 결정한 상태만 전달한다.
    // 이 Renderer가 별도의 Load/Unload 결정을 내리지는 않는다.
    std::vector<StreamingRangeChunkInfo> debugChunks;
    debugChunks.reserve(slots_.size());
    for(const auto& slot : slots_)
    {
        BoundingBox worldBounds;
        slot.Desc.Bounds.Transform(worldBounds,world);
        StreamingRangeChunkInfo info={};
        info.WorldBounds=worldBounds;
        info.Loaded=(slot.Gpu!=nullptr);
        info.Pending=slot.Pending;
        info.Desired=slot.Desired;
        info.Failed=slot.Failed;
        debugChunks.push_back(info);
    }
    rangeDebugRenderer_.SetChunks(debugChunks);
    rangeDebugRenderer_.SetFocus(focus,world);
    RefreshStats();
}

bool TerrainStreamingManager::Install(StreamingCPUResult& cpu,Slot& slot)
{
    auto resident=std::make_unique<Resident>();
    // CPU QuadTree와 Index는 결과에서 이사한다. 재생성하지 않는다.
    resident->Tree=std::move(cpu.Tree);
    const auto& m=cpu.MeshData;
    resident->FullTriangles=m.FullResolutionRange.SurfaceTriangleCount;
    resident->FullStartIndex=m.FullResolutionRange.StartIndex;
    resident->FullIndexCount=m.FullResolutionRange.IndexCount;
    if(!resident->GpuMesh.Initialize(device_.Get(),m.Vertices,m.Indices) ||
       !resident->BorderMesh.Initialize(device_.Get(),cpu.BorderVertices,cpu.BorderIndices))
        return false;
    resident->GpuBytes=
        static_cast<std::uint64_t>(m.Vertices.size())*sizeof(Vertex)+
        static_cast<std::uint64_t>(m.Indices.size())*sizeof(std::uint32_t)+
        static_cast<std::uint64_t>(cpu.BorderVertices.size())*sizeof(Vertex)+
        static_cast<std::uint64_t>(cpu.BorderIndices.size())*sizeof(std::uint32_t);
    resident->Ranges.reserve(resident->Tree.GetTotalLeafCount());
    slot.Gpu=std::move(resident);
    return true;
}

void TerrainStreamingManager::RefreshStats()
{
    stats_.DesiredChunks=0u;stats_.LoadedChunks=0u;
    stats_.PendingChunks=0u;stats_.ResidentMeshBytes=0u;
    for(const auto& slot:slots_)
    {
        stats_.DesiredChunks+=slot.Desired?1u:0u;
        stats_.PendingChunks+=slot.Pending?1u:0u;
        if(slot.Gpu)
        { ++stats_.LoadedChunks;stats_.ResidentMeshBytes+=slot.Gpu->GpuBytes; }
    }
}

void TerrainStreamingManager::Render(ID3D11DeviceContext* context,
    const XMMATRIX& view,const XMMATRIX& projection,const XMFLOAT3& cameraPosition)
{
    if(!initialized_ || !context) return;
    stats_.VisibleChunks=0u;stats_.CulledChunks=0u;stats_.DrawCalls=0u;
    stats_.RenderedSurfaceTriangles=0u;
    const XMMATRIX world=transform_.GetWorldMatrix();
    if(settings_.EnableCulling)frustum_.Build(view,projection);
    TransformCB data={};
    XMStoreFloat4x4(&data.WorldViewProjection,XMMatrixTranspose(world*view*projection));
    XMStoreFloat4x4(&data.World,XMMatrixTranspose(world));
    transforms_.Update(context,data);
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> previous;
    context->RSGetState(previous.GetAddressOf());
    context->RSSetState(solidState_.Get());
    material_.Bind(context);transforms_.BindVS(context,0u);
    for(auto& slot:slots_)
    {
        if(!slot.Gpu)continue;
        auto& chunk=*slot.Gpu;chunk.Visible=false;
        const QuadTreeLODNode* root=chunk.Tree.GetRoot();
        if(!root)continue;
        if(settings_.EnableCulling)
        {
            BoundingBox bounds;root->Bounds.Transform(bounds,world);
            if(frustum_.Contains(bounds)==DISJOINT)
            { ++stats_.CulledChunks;continue; }
        }
        chunk.Ranges.clear();chunk.Frame={};
        if(!settings_.EnableCulling && !settings_.EnableLOD)
        {
            // OFF/OFF: Chunk별 원본 해상도를 한 번씩 Draw.
            // 아래 FullResolutionRange는 worker 결과에서 보관한다.
            chunk.Ranges.push_back({chunk.FullStartIndex,chunk.FullIndexCount,chunk.FullTriangles,0u});
            chunk.Frame.RenderedSurfaceTriangles=chunk.FullTriangles;
        }
        else
            QuadTreeLODSelector::CollectDrawRanges(chunk.Tree,frustum_,world,cameraPosition,
                settings_.LOD,settings_.EnableCulling,settings_.EnableLOD,
                chunk.FullTriangles,chunk.Ranges,chunk.Frame);
        if(chunk.Ranges.empty()){++stats_.CulledChunks;continue;}
        chunk.Visible=true;++stats_.VisibleChunks;
        stats_.RenderedSurfaceTriangles+=chunk.Frame.RenderedSurfaceTriangles;
        chunk.GpuMesh.Bind(context);
        for(const auto& range:chunk.Ranges)
        { chunk.GpuMesh.DrawRange(context,range.IndexCount,range.StartIndex);++stats_.DrawCalls; }
    }
    material_.Unbind(context);
    if(showBorders_)
    {
        D3D11_PRIMITIVE_TOPOLOGY previousTopology=D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        context->IAGetPrimitiveTopology(&previousTopology);
        borderShader_.Bind(context);
        transforms_.BindVS(context,0u);
        context->RSSetState(lineState_.Get());

        if(showBorders_)
        {
            for(auto& slot:slots_)
            {
                if(!slot.Gpu || !slot.Gpu->Visible)continue;
                slot.Gpu->BorderMesh.Bind(context);
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
                slot.Gpu->BorderMesh.Draw(context);
            }
        }


        context->IASetPrimitiveTopology(previousTopology);
    }
    context->RSSetState(previous.Get());

    // 실제 Terrain / Chunk Border를 먼저 그린 뒤 3D 원기둥을 마지막에 그린다.
    // 이 별도 Debug Pass만 Depth Test OFF이므로 측면/지형 뒤에서도 범위가 보인다.
    if(showStreamingRange_)
    {
        rangeDebugRenderer_.Render(context,view,projection,world);
    }
}

bool TerrainStreamingManager::CreateRasterizers(ID3D11Device* device)
{
    D3D11_RASTERIZER_DESC rs={};
    rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_BACK;rs.DepthClipEnable=TRUE;
    if(FAILED(device->CreateRasterizerState(&rs,solidState_.GetAddressOf())))return false;
    D3D11_RASTERIZER_DESC line={};
    line.FillMode=D3D11_FILL_SOLID;line.CullMode=D3D11_CULL_NONE;
    line.DepthClipEnable=TRUE;line.AntialiasedLineEnable=TRUE;
    line.DepthBias=-256;line.SlopeScaledDepthBias=-0.25f;
    return SUCCEEDED(device->CreateRasterizerState(&line,lineState_.GetAddressOf()));
}
void TerrainStreamingManager::SetStreamingEnabled(bool v){settings_.EnableStreaming=v;}
void TerrainStreamingManager::SetCullingEnabled(bool v){settings_.EnableCulling=v;}
void TerrainStreamingManager::SetLODEnabled(bool v){settings_.EnableLOD=v;}
void TerrainStreamingManager::SetChunkBordersVisible(bool v){showBorders_=v;}
void TerrainStreamingManager::SetStreamingRangeVisible(bool v){showStreamingRange_=v;}
bool TerrainStreamingManager::IsStreamingEnabled() const{return settings_.EnableStreaming;}
bool TerrainStreamingManager::IsCullingEnabled() const{return settings_.EnableCulling;}
bool TerrainStreamingManager::IsLODEnabled() const{return settings_.EnableLOD;}
bool TerrainStreamingManager::IsChunkBordersVisible() const{return showBorders_;}
bool TerrainStreamingManager::IsStreamingRangeVisible() const{return showStreamingRange_;}
Transform& TerrainStreamingManager::GetTransform(){return transform_;}
const TerrainStreamingStats& TerrainStreamingManager::GetStats() const{return stats_;}
const std::wstring& TerrainStreamingManager::GetLastErrorMessage() const{return lastError_;}
