// ============================================================================
// PerlinNoise.h
// ----------------------------------------------------------------------------
// 2차원 Improved Perlin Noise 값을 생성하는 순수 수학 클래스.
//
// 이 클래스는 DirectX, Mesh, Terrain, Shader를 전혀 알지 못한다.
// 입력 좌표와 Seed를 기준으로 연속적인 Noise 값만 반환한다.
//
// 이렇게 분리하는 이유:
// - Terrain이 제거되어도 Noise 구현은 독립적으로 재사용 가능
// - 이후 Texture, Cloud, Mask 등 다른 Procedural 기능에도 사용 가능
// - Terrain 렌더링 코드와 Noise 수학 코드가 서로 결합되지 않음
//
// 동일한 Seed와 동일한 좌표를 사용하면 항상 동일한 값이 나온다.
// ============================================================================

#pragma once

#include <array>
#include <cstdint>

class PerlinNoise
{
public:
    // Seed를 이용하여 내부 Permutation Table을 생성한다.
    explicit PerlinNoise(std::uint32_t seed = 1337u);

    // 지정한 2D 좌표의 Perlin Noise 값을 반환한다.
    //
    // 반환값은 일반적으로 -1.0 ~ +1.0 범위에 위치하며,
    // 최종적으로 안전하게 해당 범위로 Clamp한다.
    float Noise(float x, float y) const;

private:
    // Perlin Noise의 보간 곡선.
    //
    // 6t^5 - 15t^4 + 10t^3
    //
    // 일반적인 SmoothStep보다 경계에서 1차/2차 미분값까지 자연스럽게
    // 이어지므로 Grid Cell 경계가 눈에 띄지 않게 만든다.
    static float Fade(float t);

    // 두 값 사이를 t 비율만큼 선형 보간한다.
    static float Lerp(
        float a,
        float b,
        float t);

    // Hash 값으로 Gradient 방향을 선택한 뒤,
    // 현재 점까지의 Offset Vector와 내적한다.
    static float GradientDot(
        std::uint8_t hash,
        float x,
        float y);

private:
    // 0~255 값을 Seed 기반으로 섞은 뒤 두 번 이어 붙인 Table.
    //
    // 512개를 사용하는 이유는 인접 Cell을 검색할 때
    // 매번 별도의 범위 처리 없이 연속 인덱싱하기 위해서다.
    std::array<std::uint8_t, 512> permutation_ = {};
};
