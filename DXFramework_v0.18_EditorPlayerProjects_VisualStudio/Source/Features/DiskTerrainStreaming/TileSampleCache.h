// ============================================================================
// TileSampleCache.h — CPU Tile 높이 샘플의 용량 제한 LRU 캐시.
// 디스크/Direct3D 의존성 없이 TileSamples만 보유한다.
// 호출 측(DiskTileWorker)이 Mutex로 동시 접근을 보호한다.
// ============================================================================
#pragma once
#include "Features/DiskTerrainStreaming/TileArchive.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>

class TileSampleCache
{
public:
    explicit TileSampleCache(std::size_t capacityBytes=0u)
        : capacityBytes_(capacityBytes) {}

    void SetCapacity(std::size_t bytes)
    {
        capacityBytes_=bytes;
        Trim();
    }

    bool Contains(std::uint32_t index) const
    {
        return entries_.find(index)!=entries_.end();
    }

    bool Get(std::uint32_t index,TileSamples& output)
    {
        const auto it=entries_.find(index);
        if(it==entries_.end())return false;
        output=it->second.Samples; // Build()는 읽기 전용이지만 독립 복사본 사용.
        it->second.Stamp=++clock_;
        return true;
    }

    void Put(std::uint32_t index,TileSamples samples)
    {
        const auto bytes=samples.Heights.size()*sizeof(float);
        if(capacityBytes_==0u || bytes==0u || bytes>capacityBytes_)return;
        const auto it=entries_.find(index);
        if(it!=entries_.end())
        {
            usedBytes_-=it->second.Bytes;
            entries_.erase(it);
        }
        usedBytes_+=bytes;
        entries_.emplace(index,Entry{std::move(samples),bytes,++clock_});
        Trim();
    }

    void Clear()
    {
        entries_.clear();usedBytes_=0u;clock_=0u;evictions_=0u;
    }

    std::size_t UsedBytes() const{return usedBytes_;}
    std::size_t Count() const{return entries_.size();}
    std::size_t CapacityBytes() const{return capacityBytes_;}
    std::uint64_t Evictions() const{return evictions_;}

private:
    struct Entry
    {
        TileSamples Samples;
        std::size_t Bytes=0u;
        std::uint64_t Stamp=0u;
    };
    void Trim()
    {
        while(usedBytes_>capacityBytes_ && !entries_.empty())
        {
            auto oldest=entries_.begin();
            for(auto it=entries_.begin();it!=entries_.end();++it)
                if(it->second.Stamp<oldest->second.Stamp)oldest=it;
            usedBytes_-=oldest->second.Bytes;
            entries_.erase(oldest);
            ++evictions_;
        }
    }
    std::unordered_map<std::uint32_t,Entry> entries_;
    std::size_t capacityBytes_=0u,usedBytes_=0u;
    std::uint64_t clock_=0u,evictions_=0u;
};
