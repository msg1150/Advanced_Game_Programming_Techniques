#include "Scene/Runtime/ColliderSceneAdapter.h"

ColliderSceneDocument ColliderSceneAdapter::Capture(const Collision::CollisionWorld& world)
{
    ColliderSceneDocument result;result.Colliders=world.Colliders();
    for(auto& c:result.Colliders)c.Id=0;
    return result;
}
void ColliderSceneAdapter::Apply(const ColliderSceneDocument& document,Collision::CollisionWorld& world)
{
    auto candidate=world;candidate.Clear();
    for(const auto& saved:document.Colliders)
    {
        const auto id=std::visit([&](const auto& s)->Collision::ColliderId
        {
            using T=std::decay_t<decltype(s)>;
            if constexpr(std::is_same_v<T,Collision::SphereShape>)return candidate.AddSphere(saved.Position,s.Radius);
            else if constexpr(std::is_same_v<T,Collision::AABBShape>)return candidate.AddAABB(saved.Position,s.HalfExtents);
            else if constexpr(std::is_same_v<T,Collision::OBBShape>)return candidate.AddOBB(saved.Position,s.HalfExtents);
            else return candidate.AddCapsule(saved.Position,s.Radius,s.HalfSegment);
        },saved.Shape);
        auto copy=saved;copy.Id=id;*candidate.Find(id)=std::move(copy);
    }
    candidate.UpdateCollisions();world=std::move(candidate);
}
