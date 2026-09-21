// ============================================================================
// TerrainStreamingWorker.cpp
// CPU 작업: 원본 Grid에서 한 Chunk의 Vertex만 복사하고 LOD Tree를 구성한다.
// ============================================================================
#include "Features/TerrainStreaming/TerrainStreamingWorker.h"
#include <algorithm>
#include <utility>

TerrainStreamingWorker::~TerrainStreamingWorker() { Stop(); }

bool TerrainStreamingWorker::Start(std::vector<Vertex> fullVertices,
    std::uint32_t width, std::uint32_t height, QuadTreeLODSettings lod,
    std::vector<StreamingChunkDescriptor> descriptors)
{
    Stop();
    if(width<2u || height<2u || descriptors.empty() ||
       fullVertices.size()!=static_cast<std::size_t>(width)*height) return false;
    vertices_=std::move(fullVertices);
    descriptors_=std::move(descriptors);
    width_=width; height_=height; lod_=lod;
    { std::lock_guard<std::mutex> guard(mutex_); stopping_=false; }
    try { thread_=std::thread(&TerrainStreamingWorker::ThreadMain,this); }
    catch (...) { vertices_.clear(); descriptors_.clear(); return false; }
    return true;
}

void TerrainStreamingWorker::Request(std::uint32_t index, std::uint64_t generation)
{
    { std::lock_guard<std::mutex> guard(mutex_);
      if(stopping_ || index>=descriptors_.size()) return;
      requests_.push_back({index,generation}); }
    cv_.notify_one();
}

bool TerrainStreamingWorker::TryPop(StreamingCPUResult& result)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if(completed_.empty()) return false;
    result=std::move(completed_.front()); completed_.pop_front();
    return true;
}

void TerrainStreamingWorker::Stop()
{
    { std::lock_guard<std::mutex> guard(mutex_); stopping_=true; requests_.clear(); }
    cv_.notify_all();
    if(thread_.joinable()) thread_.join();
    { std::lock_guard<std::mutex> guard(mutex_); completed_.clear(); }
    vertices_.clear(); descriptors_.clear();
}

void TerrainStreamingWorker::ThreadMain()
{
    for(;;)
    {
        RequestData request={};
        { std::unique_lock<std::mutex> lock(mutex_);
          cv_.wait(lock,[this]{return stopping_ || !requests_.empty();});
          if(stopping_) return;
          request=requests_.front(); requests_.pop_front(); }
        // 계산은 Lock 밖에서 한다. Render 스레드와 Device를 공유하지 않는다.
        StreamingCPUResult result=Prepare(request);
        { std::lock_guard<std::mutex> guard(mutex_);
          if(!stopping_) completed_.push_back(std::move(result)); }
    }
}

StreamingCPUResult TerrainStreamingWorker::Prepare(RequestData request) const
{
    StreamingCPUResult result;
    result.Index=request.Index; result.Generation=request.Generation;
    if(request.Index>=descriptors_.size()) { result.Error=L"Chunk Index 범위 오류"; return result; }
    const auto& desc=descriptors_[request.Index];
    TerrainChunkMeshData cpu;
    cpu.ChunkX=desc.ChunkX; cpu.ChunkZ=desc.ChunkZ;
    cpu.StartCellX=desc.StartX; cpu.StartCellZ=desc.StartZ;
    cpu.CellsX=desc.CellsX; cpu.CellsZ=desc.CellsZ;
    cpu.VertexWidth=desc.CellsX+1u; cpu.VertexHeight=desc.CellsZ+1u;
    cpu.Vertices.reserve(static_cast<std::size_t>(cpu.VertexWidth)*cpu.VertexHeight);
    for(std::uint32_t z=0;z<cpu.VertexHeight;++z)
        for(std::uint32_t x=0;x<cpu.VertexWidth;++x)
            cpu.Vertices.push_back(vertices_[static_cast<std::size_t>(desc.StartZ+z)*width_+desc.StartX+x]);

    // 이전 Chunk Generator와 동일하게 공유 Edge의 Vertex를 원본에서 그대로 복사한다.
    if(!result.Tree.Build(cpu.Vertices,cpu.VertexWidth,cpu.VertexHeight,
                          lod_,result.MeshData,result.Error)) return result;
    MakeBorder(cpu,desc.LastX,desc.LastZ,result.BorderVertices,result.BorderIndices);
    return result;
}

void TerrainStreamingWorker::MakeBorder(const TerrainChunkMeshData& cpu,
    bool lastX, bool lastZ, std::vector<Vertex>& vertices,
    std::vector<std::uint32_t>& indices)
{
    // v0.3 표면 경계선과 동일한 Edge 소유 규칙을 재사용한다.
    // 북/서쪽을 기본으로, 전체 지형 최종 열/행만 동/남쪽도 그린다.
    constexpr float kSurfaceOffset=0.025f;
    const bool even=((cpu.ChunkX+cpu.ChunkZ)%2u)==0u;
    const DirectX::XMFLOAT4 color=even
        ? DirectX::XMFLOAT4(1.0f,0.55f,0.15f,1.0f)
        : DirectX::XMFLOAT4(0.15f,0.85f,1.0f,1.0f);
    const auto add=[&](std::uint32_t x0,std::uint32_t z0,
                       std::uint32_t x1,std::uint32_t z1)
    {
        Vertex a=cpu.Vertices[static_cast<std::size_t>(z0)*cpu.VertexWidth+x0];
        Vertex b=cpu.Vertices[static_cast<std::size_t>(z1)*cpu.VertexWidth+x1];
        a.Position.y+=kSurfaceOffset; b.Position.y+=kSurfaceOffset;
        a.Color=color; b.Color=color;
        const auto i=static_cast<std::uint32_t>(vertices.size());
        vertices.push_back(a); vertices.push_back(b);
        indices.push_back(i); indices.push_back(i+1u);
    };
    for(std::uint32_t x=0;x<cpu.CellsX;++x) add(x,0u,x+1u,0u);
    for(std::uint32_t z=0;z<cpu.CellsZ;++z) add(0u,z,0u,z+1u);
    if(lastX) for(std::uint32_t z=0;z<cpu.CellsZ;++z)
        add(cpu.CellsX,z,cpu.CellsX,z+1u);
    if(lastZ) for(std::uint32_t x=0;x<cpu.CellsX;++x)
        add(x,cpu.CellsZ,x+1u,cpu.CellsZ);
}
