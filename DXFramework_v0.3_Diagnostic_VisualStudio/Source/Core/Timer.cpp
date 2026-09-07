// ============================================================================
// Timer.cpp
// ============================================================================

#include "Core/Timer.h"

#include <algorithm>

Timer::Timer()
{
    // QueryPerformanceFrequency는 Performance Counter의 초당 Count를 반환한다.
    LARGE_INTEGER frequency = {};
    QueryPerformanceFrequency(&frequency);

    // Count -> Seconds 변환에 매번 나눗셈을 하지 않도록
    // 역수를 미리 계산해 둔다.
    secondsPerCount_ =
        1.0 /
        static_cast<double>(frequency.QuadPart);
}

void Timer::Reset()
{
    // 현재 Performance Counter 값을 시작 시점으로 저장한다.
    QueryPerformanceCounter(&startTime_);

    previousTime_ = startTime_;
    currentTime_ = startTime_;

    deltaTime_ = 0.0f;
    totalTime_ = 0.0f;
}

void Timer::Tick()
{
    // 이번 프레임의 Counter를 읽는다.
    QueryPerformanceCounter(&currentTime_);

    const auto deltaCount =
        currentTime_.QuadPart -
        previousTime_.QuadPart;

    deltaTime_ =
        static_cast<float>(
            static_cast<double>(deltaCount) *
            secondsPerCount_);

    // Debugger Break나 Window 이동 등으로 매우 큰 DeltaTime이 들어오면
    // 카메라가 한 프레임에 크게 튈 수 있다.
    // 최대 0.1초로 제한해 이런 현상을 방어한다.
    deltaTime_ =
        std::clamp(
            deltaTime_,
            0.0f,
            0.1f);

    const auto totalCount =
        currentTime_.QuadPart -
        startTime_.QuadPart;

    totalTime_ =
        static_cast<float>(
            static_cast<double>(totalCount) *
            secondsPerCount_);

    // 다음 Tick에서 현재 프레임이 Previous가 된다.
    previousTime_ = currentTime_;
}

float Timer::GetDeltaTime() const
{
    return deltaTime_;
}

float Timer::GetTotalTime() const
{
    return totalTime_;
}
