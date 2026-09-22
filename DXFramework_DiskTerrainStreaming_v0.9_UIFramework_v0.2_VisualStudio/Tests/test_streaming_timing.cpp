// Windows / Direct3D 의존성 없이 시간 계측 누적 및 평균을 검증한다.
#include "Features/DiskTerrainStreaming/StreamingTiming.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>

int main()
{
    using namespace std::chrono;
    StreamingTiming timing;
    assert(timing.Count==0u);
    assert(timing.AverageMilliseconds()==0.0);
    assert(timing.LastMilliseconds()==0.0);
    timing.Record(microseconds(500));
    timing.Record(microseconds(1500));
    assert(timing.Count==2u);
    assert(timing.TotalMicroseconds==2000u);
    assert(timing.LastMicroseconds==1500u);
    assert(timing.MaxMicroseconds==1500u);
    assert(std::abs(timing.LastMilliseconds()-1.5)<1e-10);
    assert(std::abs(timing.AverageMilliseconds()-1.0)<1e-10);
    assert(std::abs(timing.TotalMilliseconds()-2.0)<1e-10);
    timing.Record(microseconds(-5)); // 타이머 역전/잘못된 구간 방어.
    assert(timing.Count==3u && timing.LastMicroseconds==0u);
    assert(timing.TotalMicroseconds==2000u);
    std::cout << "PASS: zero samples, last/average/total/max, invalid negative duration" << std::endl;
}
