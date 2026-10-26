// ============================================================================
// DiskTileWorker.h — File I/O + CPU LOD 생성 전용 1개 Worker.
// Direct3D Device/Context와 GPU Resource에는 접근하지 않는다.
// ============================================================================
#pragma once
#include "Features/DiskTerrainStreaming/TileGeometryBuilder.h"
#include "Features/DiskTerrainStreaming/TileSampleCache.h"
#include "Features/DiskTerrainStreaming/StreamingTiming.h"
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
    // Main 스레드에서 매 프레임 최신 Prefetch 목표 목록을 전달한다.
    // 기존 큐를 교체하므로 방향 변경 후 오래된 대기 요청은 남지 않는다.
    void SetPrefetchTargets(const std::vector<std::uint32_t>& targets);
    void SetCacheCapacity(std::size_t bytes); // 0: 캐시 비활성화 및 전체 비우기
    struct Metrics
    {
        // CacheHits/Misses는 실제 GPU 로드용 요청의 조회만 센다.
        // CPU Cache가 OFF면 조회하지 않으므로 두 값 모두 증가하지 않는다.
        std::uint64_t DiskReads=0u,DiskBytes=0u,CacheHits=0u,CacheMisses=0u;
        // 정상적으로 완료한 TileArchive::ReadTile 1회 구간만 측정.
        // Prefetch 읽기도 포함하며 실패한 파일 읽기는 DiskReads와 함께 제외.
        StreamingTiming DiskReadTime;
        std::uint64_t PrefetchReads=0u,PrefetchFailures=0u;
        std::uint64_t CacheEvictions=0u;
        std::uint64_t CachedBytes=0u;
        std::uint32_t CachedTiles=0u,PrefetchQueued=0u;
    };
    Metrics GetMetrics();
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
    std::deque<Job> requests_; // 필수 로딩은 Prefetch보다 항상 우선한다.
    std::deque<std::uint32_t> prefetchQueue_;
    std::vector<bool> prefetchWanted_;
    std::vector<bool> prefetchFailed_; // 실패한 speculative 파일은 목표가 바뀔 때까지 재시도 안 함.
    std::deque<TilePrepared> ready_;
    TileSampleCache cache_;
    Metrics metrics_;

    std::vector<std::uint64_t> latestGeneration_;
    bool stopping_=false;
};
