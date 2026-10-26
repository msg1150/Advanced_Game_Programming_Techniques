// ============================================================================
// StreamingTiming.h — std::chrono::steady_clock 기반 누적 시간 통계.
// OS/DirectX 독립, 호출하는 스레드가 소유하고 필요한 시점에 Snapshot을 복사한다.
// ============================================================================
#pragma once
#include <chrono>
#include <cstdint>

struct StreamingTiming
{
    std::uint64_t Count=0u;
    std::uint64_t TotalMicroseconds=0u;
    std::uint64_t LastMicroseconds=0u;
    std::uint64_t MaxMicroseconds=0u;

    void Record(std::chrono::steady_clock::duration elapsed) noexcept
    {
        const auto signedUs=std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
        const auto microseconds=static_cast<std::uint64_t>(signedUs>0 ? signedUs : 0);
        ++Count;
        TotalMicroseconds+=microseconds;
        LastMicroseconds=microseconds;
        if(microseconds>MaxMicroseconds)MaxMicroseconds=microseconds;
    }

    double LastMilliseconds() const noexcept
    {
        return static_cast<double>(LastMicroseconds)/1000.0;
    }
    double AverageMilliseconds() const noexcept
    {
        return Count==0u ? 0.0 : static_cast<double>(TotalMicroseconds)/
                                   (1000.0*static_cast<double>(Count));
    }
    double TotalMilliseconds() const noexcept
    {
        return static_cast<double>(TotalMicroseconds)/1000.0;
    }
};
