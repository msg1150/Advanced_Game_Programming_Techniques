// ============================================================================
// DiskTileWorker.h — File I/O + CPU LOD 생성 전용 1개 Worker.
// Direct3D Device/Context와 GPU Resource에는 접근하지 않는다.
// ============================================================================
#pragma once
#include "Features/DiskTerrainStreaming/TileGeometryBuilder.h"
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>
class DiskTileWorker
{
public:
    DiskTileWorker()=default;
    ~DiskTileWorker();
    DiskTileWorker(const DiskTileWorker&)=delete;
    DiskTileWorker& operator=(const DiskTileWorker&)=delete;
    bool Start(std::filesystem::path directory,TileWorldInfo world,QuadTreeLODSettings lod);
    void Request(std::uint32_t index,std::uint64_t generation);
    // 요청 취소: Queue에 있는 오래된 Generation은 CPU 작업 전에 폐기한다.
    void Cancel(std::uint32_t index,std::uint64_t nextGeneration);
    bool TryPop(TilePrepared& result);
    void Stop();
private:
    struct Job {std::uint32_t Index;std::uint64_t Generation;};
    void Main();
    std::filesystem::path directory_;
    TileWorldInfo world_;
    QuadTreeLODSettings lod_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Job> requests_;
    std::deque<TilePrepared> ready_;
    std::vector<std::uint64_t> latestGeneration_;
    bool stopping_=false;
};
