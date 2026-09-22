// ============================================================================
// DiskTileWorker.cpp — 필수 Tile 우선 + 저우선순위 Prefetch + 용량 제한 LRU.
// Worker만 디스크 파일을 읽고 CPU Geometry를 구성하며 D3D API를 호출하지 않는다.
// Queue, Cache, Metrics 접근은 mutex_로 보호한다.
// ============================================================================
#include "Features/DiskTerrainStreaming/DiskTileWorker.h"
#include <algorithm>
#include <chrono>
#include <utility>

DiskTileWorker::~DiskTileWorker(){Stop();}
bool DiskTileWorker::Start(std::filesystem::path directory,TileWorldInfo world,
                           QuadTreeLODSettings lod)
{
    Stop();
    if(world.Tiles.empty())return false;
    directory_=std::move(directory);world_=std::move(world);lod_=lod;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        latestGeneration_.assign(world_.Tiles.size(),0u);
        prefetchWanted_.assign(world_.Tiles.size(),false);
        prefetchFailed_.assign(world_.Tiles.size(),false);
        metrics_={};stopping_=false;
        cache_.Clear();
    }
    try {thread_=std::thread(&DiskTileWorker::Main,this);}
    catch(...) {Stop();return false;}
    return true;
}
void DiskTileWorker::Request(std::uint32_t index,std::uint64_t generation)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_ || index>=latestGeneration_.size())return;
        latestGeneration_[index]=generation;
        requests_.push_back({index,generation});
        // 필수 요청이 된 Tile의 speculative 큐는 제거한다.
        prefetchWanted_[index]=false;
        prefetchQueue_.erase(std::remove(prefetchQueue_.begin(),prefetchQueue_.end(),index),
                             prefetchQueue_.end());
    }
    wake_.notify_one();
}
void DiskTileWorker::Cancel(std::uint32_t index,std::uint64_t generation)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(index<latestGeneration_.size())latestGeneration_[index]=generation;
}
bool DiskTileWorker::TryPop(TilePrepared& result)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(ready_.empty())return false;
    result=std::move(ready_.front());ready_.pop_front();return true;
}
void DiskTileWorker::SetPrefetchTargets(const std::vector<std::uint32_t>& targets)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_)return;
        std::vector<bool> nextWanted(prefetchWanted_.size(),false);
        // 캐시가 꺼져 있다면 단순히 디스크를 읽고 버리는 예측 작업 금지.
        if(cache_.CapacityBytes()>0u)
            for(const auto index:targets)
                if(index<nextWanted.size())nextWanted[index]=true;
        for(std::size_t i=0;i<nextWanted.size();++i)
            // 실패 중 같은 예측 영역에 머무는 동안 재시도하지 않고,
            // 그 영역에서 빠져나오거나 새로 진입할 때만 재시도 허용.
            if(prefetchWanted_[i]!=nextWanted[i])prefetchFailed_[i]=false;
        prefetchWanted_=std::move(nextWanted);
        prefetchQueue_.clear();
        // targets는 Manager가 예측 위치와 가까운 순으로 정렬하여 전달한다.
        for(const auto index:targets)
        {
            if(index>=prefetchWanted_.size() || !prefetchWanted_[index] ||
               prefetchFailed_[index] || cache_.Contains(index))continue;
            prefetchQueue_.push_back(index);
        }
    }
    wake_.notify_one();
}
void DiskTileWorker::SetCacheCapacity(std::size_t bytes)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.SetCapacity(bytes);
        if(bytes==0u)
        {
            prefetchQueue_.clear();
            std::fill(prefetchWanted_.begin(),prefetchWanted_.end(),false);
            std::fill(prefetchFailed_.begin(),prefetchFailed_.end(),false);
        }
    }
    wake_.notify_one();
}
DiskTileWorker::Metrics DiskTileWorker::GetMetrics()
{
    std::lock_guard<std::mutex> lock(mutex_);
    Metrics result=metrics_;
    result.CacheEvictions=cache_.Evictions();
    result.CachedBytes=cache_.UsedBytes();
    result.CachedTiles=static_cast<std::uint32_t>(cache_.Count());
    result.PrefetchQueued=static_cast<std::uint32_t>(prefetchQueue_.size());
    return result;
}
void DiskTileWorker::Stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_=true;requests_.clear();prefetchQueue_.clear();
    }
    wake_.notify_all();
    if(thread_.joinable())thread_.join();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ready_.clear();latestGeneration_.clear();prefetchWanted_.clear();
        prefetchFailed_.clear();cache_.Clear();metrics_={};
    }
    world_={};
}
void DiskTileWorker::Main()
{
    for(;;)
    {
        Job job={};
        bool speculative=false;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock,[this]{return stopping_ || !requests_.empty() ||
                                            !prefetchQueue_.empty();});
            if(stopping_)return;
            // 현재 디스크 작업은 선점할 수 없지만, 다음 작업은 항상 필수 요청 우선.
            if(!requests_.empty())
            {
                job=requests_.front();requests_.pop_front();
                if(job.Index>=latestGeneration_.size() ||
                   latestGeneration_[job.Index]!=job.Generation)continue;
            }
            else
            {
                speculative=true;
                job.Index=prefetchQueue_.front();prefetchQueue_.pop_front();
                if(job.Index>=prefetchWanted_.size() || !prefetchWanted_[job.Index] ||
                   cache_.Contains(job.Index))continue;
            }
        }

        TileSamples samples;
        bool fromCache=false;
        if(!speculative)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(cache_.CapacityBytes()>0u)
            {
                fromCache=cache_.Get(job.Index,samples);
                if(fromCache)++metrics_.CacheHits;
                else ++metrics_.CacheMisses;
            }
        }
        std::wstring error;
        std::uint64_t diskBytes=0u;
        if(!fromCache)
        {
            const auto x=job.Index%world_.TilesX;
            const auto z=job.Index/world_.TilesX;
            // 파일 열기 + 헤더/샘플 읽기 + 유효성 검사까지의 실측 시간.
            // 큐 대기 / CPU Mesh 생성 / GPU 업로드는 이 시간에 포함하지 않는다.
            const auto diskStart=std::chrono::steady_clock::now();
            const bool readSucceeded=TileArchive::ReadTile(
                directory_,world_,x,z,samples,error);
            const auto diskElapsed=std::chrono::steady_clock::now()-diskStart;
            if(!readSucceeded)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if(speculative)
                {
                    ++metrics_.PrefetchFailures;
                    if(job.Index<prefetchFailed_.size())prefetchFailed_[job.Index]=true;
                }
            }
            else
            {
                diskBytes=24ull+samples.Heights.size()*sizeof(float);
                std::lock_guard<std::mutex> lock(mutex_);
                ++metrics_.DiskReads;
                metrics_.DiskBytes+=diskBytes;
                metrics_.DiskReadTime.Record(diskElapsed);
                if(speculative)++metrics_.PrefetchReads;
                // 방향이 바뀐 과거 speculative 작업은 메모리에 넣지 않는다.
                const bool stillWanted=!speculative ||
                    (job.Index<prefetchWanted_.size() && prefetchWanted_[job.Index]);
                if(stillWanted)
                {
                    cache_.Put(job.Index,samples);
                }
            }
        }
        if(speculative)continue; // Prefetch는 GPU 업로드/LOD 생성 금지.

        TilePrepared result;
        result.Index=job.Index;result.Generation=job.Generation;
        result.DiskBytes=diskBytes;
        result.Error=std::move(error);
        if(result.Error.empty())TileGeometryBuilder::Build(world_,samples,lod_,result);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(stopping_)return;
            if(job.Index<latestGeneration_.size() &&
               latestGeneration_[job.Index]==job.Generation)
                ready_.push_back(std::move(result));
        }
    }
}
