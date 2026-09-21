// ============================================================================
// TerrainStreamingWorker.h
// ----------------------------------------------------------------------------
// CPU 전용 비동기 작업자. HeightMap 전체 Vertex는 읽기 전용 원본으로 유지한다.
// 요청된 Chunk만 잘라 QuadTreeLOD + 경계 Vertex를 작업 스레드에서 생성한다.
// Direct3D Device / Context / GPU 리소스에는 절대 접근하지 않는다.
// ============================================================================
#pragma once
#include "Features/HeightMapTerrain/HeightMapTerrainGenerator.h"
#include "Features/QuadTreeLOD/QuadTreeLOD.h"
#include "Features/TerrainChunk/TerrainChunkGenerator.h"
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct StreamingChunkDescriptor
{
    std::uint32_t Index=0u, ChunkX=0u, ChunkZ=0u;
    std::uint32_t StartX=0u, StartZ=0u, CellsX=0u, CellsZ=0u;
    DirectX::BoundingBox Bounds;
    bool LastX=false, LastZ=false;
};

struct StreamingCPUResult
{
    std::uint32_t Index=0u;
    std::uint64_t Generation=0u;
    QuadTreeLOD Tree;
    QuadTreeLODMeshData MeshData;
    std::vector<Vertex> BorderVertices;
    std::vector<std::uint32_t> BorderIndices;
    std::wstring Error;
};

class TerrainStreamingWorker
{
public:
    TerrainStreamingWorker()=default;
    ~TerrainStreamingWorker();
    TerrainStreamingWorker(const TerrainStreamingWorker&)=delete;
    TerrainStreamingWorker& operator=(const TerrainStreamingWorker&)=delete;

    bool Start(std::vector<Vertex> fullVertices,
               std::uint32_t width, std::uint32_t height,
               QuadTreeLODSettings lod,
               std::vector<StreamingChunkDescriptor> descriptors);
    void Request(std::uint32_t index, std::uint64_t generation);
    bool TryPop(StreamingCPUResult& outResult);
    void Stop();

private:
    struct RequestData { std::uint32_t Index; std::uint64_t Generation; };
    void ThreadMain();
    StreamingCPUResult Prepare(RequestData request) const;
    static void MakeBorder(const TerrainChunkMeshData& cpu, bool lastX, bool lastZ,
                           std::vector<Vertex>& vertices,
                           std::vector<std::uint32_t>& indices);

    // Start 이후 Stop 전까지 불변이며 GPU 스레드와 동시 변경하지 않는다.
    std::vector<Vertex> vertices_;
    std::vector<StreamingChunkDescriptor> descriptors_;
    std::uint32_t width_=0u, height_=0u;
    QuadTreeLODSettings lod_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<RequestData> requests_;
    std::deque<StreamingCPUResult> completed_;
    bool stopping_=false;
};
