// ============================================================================
// QuadTreeLODGeomorphPolicy.h
// ----------------------------------------------------------------------------
// QuadTree LOD Geomorphing에서 "얼마나 Parent 모양으로 붙일지"만 계산하는
// 플랫폼 독립 정책 모듈이다.
//
// 이 파일은 DirectX / Renderer / Terrain 자료구조를 포함하지 않는다.
// 따라서 Geomorph 기능의 거리 정책만 단독 테스트할 수 있고,
// Geomorph Feature를 제거해도 기존 QuadTreeLOD 자체에는 영향이 없다.
// ============================================================================
#pragma once

#include <algorithm>
#include <cmath>

namespace QuadTreeLODGeomorphPolicy
{
    struct Settings
    {
        // Parent가 Child로 분할되는 경계 바로 안쪽에서 Morph를 시작한다.
        //
        // 0.25f라면 Parent SplitDistance의 마지막 25% 구간에서
        // Child Mesh가 Parent 표면 -> 자기 원본 표면으로 부드럽게 복귀한다.
        //
        // 0.0f  : 사실상 즉시 전환 (Popping 완화 효과 없음)
        // 0.25f : 기본값
        // 0.49f : 허용 가능한 가장 긴 구간에 가까움
        //
        // 0.5 이상을 막는 이유:
        // QuadTree Child 크기는 Parent의 약 절반이므로 Transition을 절반 이상
        // 사용하면 다음 Child Split 구간과 겹쳐 연속 전이가 깨질 수 있다.
        float TransitionRatio = 0.25f;
    };

    inline bool IsValid(const Settings& settings)
    {
        return std::isfinite(settings.TransitionRatio) &&
               settings.TransitionRatio >= 0.0f &&
               settings.TransitionRatio < 0.5f;
    }

    // distanceToParentBounds:
    //   Camera에서 Parent Node AABB까지의 최단 거리.
    //
    // parentSplitDistance:
    //   기존 QuadTreeLODSelector와 동일한
    //   ParentWorldSize * SplitDistanceFactor.
    //
    // 반환값:
    //   0 = Child 원본 높이/Normal을 그대로 사용.
    //   1 = Child를 Parent 표면에 완전히 붙임.
    //
    // Parent가 막 Child로 분할되는 순간에는 1이 되고,
    // Camera가 더 가까워질수록 0으로 내려가므로 LOD 전환 순간의 Popping을 줄인다.
    inline float CalculateMorphFactor(
        float distanceToParentBounds,
        float parentSplitDistance,
        float transitionRatio)
    {
        if (!std::isfinite(distanceToParentBounds) ||
            !std::isfinite(parentSplitDistance) ||
            !std::isfinite(transitionRatio) ||
            parentSplitDistance <= 0.0f ||
            transitionRatio <= 0.0f)
        {
            return 0.0f;
        }

        const float clampedRatio =
            std::clamp(transitionRatio, 0.0f, 0.499f);

        const float transitionStart =
            parentSplitDistance * (1.0f - clampedRatio);

        const float transitionLength =
            std::max(parentSplitDistance - transitionStart, 0.0001f);

        return std::clamp(
            (distanceToParentBounds - transitionStart) / transitionLength,
            0.0f,
            1.0f);
    }
}
