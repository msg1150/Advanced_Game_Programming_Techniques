// ============================================================================
// DiskTileWorker.cpp
// ============================================================================
#include "Features/DiskTerrainStreaming/DiskTileWorker.h"
#include <utility>
DiskTileWorker::~DiskTileWorker(){Stop();}
bool DiskTileWorker::Start(std::filesystem::path directory,TileWorldInfo world,
                           QuadTreeLODSettings lod)
{
    Stop();
    if(world.Tiles.empty())return false;
    directory_=std::move(directory);world_=std::move(world);lod_=lod;
    latestGeneration_.assign(world_.Tiles.size(),0u);
    {std::lock_guard<std::mutex> lock(mutex_);stopping_=false;}
    try { thread_=std::thread(&DiskTileWorker::Main,this); }
    catch(...) {return false;}
    return true;
}
void DiskTileWorker::Request(std::uint32_t index,std::uint64_t generation)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_ || index>=latestGeneration_.size())return;
        latestGeneration_[index]=generation;
        requests_.push_back({index,generation});
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
void DiskTileWorker::Stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_=true;requests_.clear();
    }
    wake_.notify_all();
    if(thread_.joinable())thread_.join();
    {std::lock_guard<std::mutex> lock(mutex_);ready_.clear();}
    latestGeneration_.clear();world_={};
}
void DiskTileWorker::Main()
{
    for(;;)
    {
        Job job={};
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock,[this]{return stopping_ || !requests_.empty();});
            if(stopping_)return;
            job=requests_.front();requests_.pop_front();
            if(job.Index>=latestGeneration_.size() ||
               latestGeneration_[job.Index]!=job.Generation)continue;
        }
        TilePrepared result;result.Index=job.Index;result.Generation=job.Generation;
        const auto x=job.Index%world_.TilesX;
        const auto z=job.Index/world_.TilesX;
        TileSamples samples;
        if(TileArchive::ReadTile(directory_,world_,x,z,samples,result.Error))
        {
            result.DiskBytes=24u+samples.Heights.size()*sizeof(float);
            TileGeometryBuilder::Build(world_,samples,lod_,result);
        }
        // samples / MeshData는 result 수명과 함께 정리되고, GPU에는 Worker가 접근하지 않는다.
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(stopping_)return;
            if(latestGeneration_[job.Index]==job.Generation)
                ready_.push_back(std::move(result));
        }
    }
}
