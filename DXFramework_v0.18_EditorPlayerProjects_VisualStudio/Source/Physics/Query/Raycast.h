// ============================================================================
// Raycast.h
// ----------------------------------------------------------------------------
// CPU Scene Query용 Raycast. GPU Picking과 목적이 다르며 Collision Shape의
// 수학을 재사용한다. Gameplay/AI/Debug Line Trace에 사용할 수 있다.
// ============================================================================
#pragma once

#include "Physics/Collision/Core/CollisionTypes.h"

#include <vector>

namespace Collision
{
    struct Ray
    {
        Vec3 Origin{};
        Vec3 Direction{0.f,0.f,1.f};
    };

    struct RaycastHit
    {
        bool Hit=false;
        ColliderId Collider=0;
        float Distance=0.f;
        Vec3 Point{};
        Vec3 Normal{};
    };

    class Raycast
    {
    public:
        static RaycastHit Test(const Ray& ray,const ColliderComponent& collider,float maxDistance=100000.f);
        static RaycastHit Closest(const Ray& ray,const std::vector<ColliderComponent>& colliders,
                                  LayerMask mask=LayerAll,float maxDistance=100000.f);
    };
}
