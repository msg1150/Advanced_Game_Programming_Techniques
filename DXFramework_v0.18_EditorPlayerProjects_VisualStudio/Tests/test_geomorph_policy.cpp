#include "Features/QuadTreeLODGeomorph/QuadTreeLODGeomorphPolicy.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    using namespace QuadTreeLODGeomorphPolicy;

    Settings settings;
    settings.TransitionRatio = 0.25f;
    assert(IsValid(settings));

    // SplitDistance 100, TransitionRatio 25% -> 75~100 구간에서 0~1.
    assert(std::abs(CalculateMorphFactor(75.0f, 100.0f, 0.25f) - 0.0f) < 0.0001f);
    assert(std::abs(CalculateMorphFactor(87.5f, 100.0f, 0.25f) - 0.5f) < 0.0001f);
    assert(std::abs(CalculateMorphFactor(100.0f, 100.0f, 0.25f) - 1.0f) < 0.0001f);

    // 범위를 벗어나도 0~1 Clamp.
    assert(CalculateMorphFactor(0.0f, 100.0f, 0.25f) == 0.0f);
    assert(CalculateMorphFactor(150.0f, 100.0f, 0.25f) == 1.0f);

    // TransitionRatio 0은 기존 즉시 LOD 전환과 같은 동작.
    assert(CalculateMorphFactor(100.0f, 100.0f, 0.0f) == 0.0f);

    Settings invalid;
    invalid.TransitionRatio = 0.5f;
    assert(!IsValid(invalid));

    std::cout << "PASS geomorph policy\n";
    return 0;
}
