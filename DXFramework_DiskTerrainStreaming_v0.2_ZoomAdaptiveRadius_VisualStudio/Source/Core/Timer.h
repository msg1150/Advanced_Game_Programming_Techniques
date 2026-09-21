// ============================================================================
// Timer.h
// ----------------------------------------------------------------------------
// QueryPerformanceCounter 기반 고해상도 프레임 Timer.
//
// DeltaTime을 사용하면 FPS가 달라져도
// 이동 속도와 같은 시간 기반 로직을 일정하게 유지할 수 있다.
// ============================================================================

#pragma once

#include <Windows.h>

class Timer
{
public:
    Timer();

    // 현재 시점을 Timer 시작점으로 초기화한다.
    void Reset();

    // 한 프레임이 지날 때마다 호출하여
    // DeltaTime / TotalTime을 새로 계산한다.
    void Tick();

    // 직전 프레임과 현재 프레임 사이의 시간(초).
    float GetDeltaTime() const;

    // Reset 이후 누적된 전체 시간(초).
    float GetTotalTime() const;

private:
    // Performance Counter 1 Count가 몇 초인지 나타내는 변환 계수.
    double secondsPerCount_ = 0.0;

    LARGE_INTEGER startTime_ = {};
    LARGE_INTEGER previousTime_ = {};
    LARGE_INTEGER currentTime_ = {};

    float deltaTime_ = 0.0f;
    float totalTime_ = 0.0f;
};
