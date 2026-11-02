// ============================================================================
// CollisionDetector.h
// ----------------------------------------------------------------------------
// Shape Pair별 Narrow Phase 알고리즘의 단일 진입점.
// std::variant Visitor를 사용하므로 GameObject/Renderer 계층과 분리된다.
// ============================================================================
#pragma once

#include "Physics/Collision/Core/CollisionResult.h"

namespace Collision
{
    class CollisionDetector
    {
    public:
        static CollisionResult Test(const ColliderComponent& a,const ColliderComponent& b);
    };
}
