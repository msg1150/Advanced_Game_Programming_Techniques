// ============================================================================
// PerlinNoise.cpp
// ============================================================================

#include "Features/PerlinTerrain/PerlinNoise.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <random>

PerlinNoise::PerlinNoise(std::uint32_t seed)
{
    // ------------------------------------------------------------------------
    // 0 ~ 255까지의 기본 Permutation 값을 만든다.
    // ------------------------------------------------------------------------
    std::array<std::uint8_t, 256> basePermutation = {};

    for (std::size_t index = 0;
         index < basePermutation.size();
         ++index)
    {
        basePermutation[index] =
            static_cast<std::uint8_t>(index);
    }

    // ------------------------------------------------------------------------
    // 동일 Seed라면 항상 같은 순서가 나오도록
    // std::mt19937을 사용해 Permutation을 섞는다.
    // ------------------------------------------------------------------------
    std::mt19937 randomEngine(seed);

    std::shuffle(
        basePermutation.begin(),
        basePermutation.end(),
        randomEngine);

    // ------------------------------------------------------------------------
    // 256개 Table을 두 번 반복해 512개로 만든다.
    //
    // 예:
    // [0 ... 255][0 ... 255]
    //
    // 이 구조 덕분에 Hash Lookup 과정에서
    // 255 경계를 넘어가는 인덱스를 간단하게 처리할 수 있다.
    // ------------------------------------------------------------------------
    for (std::size_t index = 0;
         index < permutation_.size();
         ++index)
    {
        permutation_[index] =
            basePermutation[index & 255u];
    }
}

float PerlinNoise::Noise(float x, float y) const
{
    // ------------------------------------------------------------------------
    // 1. 현재 좌표가 들어 있는 Integer Grid Cell을 찾는다.
    //
    // floor를 먼저 사용하는 이유는 음수 좌표에서도
    // Cell 위치가 올바르게 계산되도록 하기 위해서다.
    // ------------------------------------------------------------------------
    const int floorX =
        static_cast<int>(std::floor(x));

    const int floorY =
        static_cast<int>(std::floor(y));

    // Permutation Table은 256 단위로 반복된다.
    const std::uint32_t cellX0 =
        static_cast<std::uint32_t>(floorX) & 255u;

    const std::uint32_t cellY0 =
        static_cast<std::uint32_t>(floorY) & 255u;

    const std::uint32_t cellX1 =
        (cellX0 + 1u) & 255u;

    const std::uint32_t cellY1 =
        (cellY0 + 1u) & 255u;

    // ------------------------------------------------------------------------
    // 2. 현재 Cell 내부의 Local 좌표를 0~1 범위로 구한다.
    // ------------------------------------------------------------------------
    const float localX =
        x - static_cast<float>(floorX);

    const float localY =
        y - static_cast<float>(floorY);

    // 반대쪽 Corner에서 바라본 Offset.
    const float localXMinusOne =
        localX - 1.0f;

    const float localYMinusOne =
        localY - 1.0f;

    // ------------------------------------------------------------------------
    // 3. Fade Curve를 적용한다.
    //
    // 단순히 localX/localY로 Lerp하면 Cell 경계가 눈에 띌 수 있다.
    // Fade를 적용한 값을 보간 비율로 사용하면 훨씬 부드럽게 이어진다.
    // ------------------------------------------------------------------------
    const float fadeX =
        Fade(localX);

    const float fadeY =
        Fade(localY);

    // ------------------------------------------------------------------------
    // 4. 현재 Cell의 네 Corner에 대해 Seed 기반 Hash를 얻는다.
    //
    // 좌표 자체에 Random 값을 저장하는 것이 아니라
    // Permutation Table을 두 번 조회하여 재현 가능한 Hash를 만든다.
    // ------------------------------------------------------------------------
    const std::uint8_t hash00 =
        permutation_[
            static_cast<std::size_t>(
                permutation_[cellX0]) +
            cellY0];

    const std::uint8_t hash10 =
        permutation_[
            static_cast<std::size_t>(
                permutation_[cellX1]) +
            cellY0];

    const std::uint8_t hash01 =
        permutation_[
            static_cast<std::size_t>(
                permutation_[cellX0]) +
            cellY1];

    const std::uint8_t hash11 =
        permutation_[
            static_cast<std::size_t>(
                permutation_[cellX1]) +
            cellY1];

    // ------------------------------------------------------------------------
    // 5. 각 Corner Gradient와 현재 점까지의 Vector를 내적한다.
    // ------------------------------------------------------------------------
    const float dot00 =
        GradientDot(
            hash00,
            localX,
            localY);

    const float dot10 =
        GradientDot(
            hash10,
            localXMinusOne,
            localY);

    const float dot01 =
        GradientDot(
            hash01,
            localX,
            localYMinusOne);

    const float dot11 =
        GradientDot(
            hash11,
            localXMinusOne,
            localYMinusOne);

    // ------------------------------------------------------------------------
    // 6. X 방향으로 먼저 보간하고,
    // 다시 Y 방향으로 보간해 최종 Noise 값을 만든다.
    // ------------------------------------------------------------------------
    const float lower =
        Lerp(
            dot00,
            dot10,
            fadeX);

    const float upper =
        Lerp(
            dot01,
            dot11,
            fadeX);

    const float result =
        Lerp(
            lower,
            upper,
            fadeY);

    // Floating Point 오차까지 고려해 최종 값을 안전하게 제한한다.
    return std::clamp(
        result,
        -1.0f,
        1.0f);
}

float PerlinNoise::Fade(float t)
{
    // 6t^5 - 15t^4 + 10t^3
    return
        t * t * t *
        (
            t *
            (
                t * 6.0f -
                15.0f
            ) +
            10.0f
        );
}

float PerlinNoise::Lerp(
    float a,
    float b,
    float t)
{
    return
        a +
        (b - a) * t;
}

float PerlinNoise::GradientDot(
    std::uint8_t hash,
    float x,
    float y)
{
    // ------------------------------------------------------------------------
    // 8개의 2D 단위 Gradient 방향을 사용한다.
    //
    // Axis 방향 4개:
    // (+X), (-X), (+Y), (-Y)
    //
    // Diagonal 방향 4개:
    // (+X,+Y), (-X,+Y), (+X,-Y), (-X,-Y)
    //
    // 대각선은 길이가 sqrt(2)가 되므로 1/sqrt(2)를 곱해
    // 모든 Gradient의 길이를 가능한 한 일정하게 맞춘다.
    // ------------------------------------------------------------------------
    constexpr float inverseSqrtTwo =
        0.70710678118f;

    switch (hash & 7u)
    {
    case 0:
        return x;

    case 1:
        return -x;

    case 2:
        return y;

    case 3:
        return -y;

    case 4:
        return
            (x + y) *
            inverseSqrtTwo;

    case 5:
        return
            (-x + y) *
            inverseSqrtTwo;

    case 6:
        return
            (x - y) *
            inverseSqrtTwo;

    default:
        return
            (-x - y) *
            inverseSqrtTwo;
    }
}
