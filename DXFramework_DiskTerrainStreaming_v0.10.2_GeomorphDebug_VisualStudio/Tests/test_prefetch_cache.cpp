// 플랫폼 독립 테스트: 선행 위치 예측, CPU Samples Cache 용량/LRU, 비활성화.
#include "Features/DiskTerrainStreaming/PrefetchPolicy.h"
#include "Features/DiskTerrainStreaming/TileSampleCache.h"
#include <cassert>
#include <cmath>
#include <iostream>

static TileSamples Sample(float value)
{
    TileSamples s;
    s.Heights.assign(10u,value);
    return s;
}
int main()
{
    using PrefetchPolicy::PredictOffset;
    const auto forward=PredictOffset(30.f,0.f,.9f,48.f);
    assert(std::abs(forward.X-27.f)<.0001f && forward.Z==0.f);
    const auto capped=PredictOffset(300.f,0.f,1.f,48.f);
    assert(std::abs(capped.X-48.f)<.0001f);
    assert(PredictOffset(0.f,0.f,1.f,48.f).X==0.f);
    assert(PredictOffset(30.f,0.f,1.f,0.f).X==0.f);
    assert(PredictOffset(0.f,-20.f,.5f,48.f).Z==-10.f);
    TileSampleCache c(80u); // 2 Tile * 10 float * sizeof(float)
    c.Put(1u,Sample(1.f));
    c.Put(2u,Sample(2.f));
    assert(c.Count()==2u && c.UsedBytes()==80u);
    TileSamples out;
    assert(c.Get(1u,out) && out.Heights[0]==1.f); // 1을 최근 사용함
    c.Put(3u,Sample(3.f));
    assert(c.Contains(1u) && c.Contains(3u) && !c.Contains(2u));
    assert(c.Evictions()==1u);
    c.SetCapacity(40u);
    assert(c.Count()==1u && c.UsedBytes()==40u && c.Evictions()==2u);
    c.SetCapacity(0u);
    assert(c.Count()==0u && c.UsedBytes()==0u);
    c.Put(99u,Sample(99.f));
    assert(c.Count()==0u);
    std::cout << "PASS: prediction, lead cap, cache LRU, budget, disable" << std::endl;
}
