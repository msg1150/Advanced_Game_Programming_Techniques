// ============================================================================
// CollisionWorld.cpp
// ============================================================================
#include "Physics/Collision/Core/CollisionWorld.h"
#include "Physics/Collision/Detection/CollisionDetector.h"

#include <algorithm>
#include <utility>

namespace Collision
{
    ColliderId CollisionWorld::Add(ShapeData shape,const Vec3& position,const wchar_t* baseName)
    {
        ColliderComponent collider;
        collider.Id=nextId_++;
        collider.Name=std::wstring(baseName)+L" #"+std::to_wstring(collider.Id);
        collider.Position=position;
        collider.Shape=std::move(shape);
        colliders_.push_back(std::move(collider));
        return colliders_.back().Id;
    }

    ColliderId CollisionWorld::AddSphere(const Vec3& p,float radius)
    {
        return Add(SphereShape{std::max(0.01f,radius)},p,L"Sphere");
    }
    ColliderId CollisionWorld::AddAABB(const Vec3& p,const Vec3& e)
    {
        return Add(AABBShape{Max(Abs(e),{0.01f,0.01f,0.01f})},p,L"AABB");
    }
    ColliderId CollisionWorld::AddOBB(const Vec3& p,const Vec3& e)
    {
        return Add(OBBShape{Max(Abs(e),{0.01f,0.01f,0.01f})},p,L"OBB");
    }
    ColliderId CollisionWorld::AddCapsule(const Vec3& p,float radius,float halfSegment)
    {
        return Add(CapsuleShape{std::max(0.01f,radius),std::max(0.f,halfSegment)},p,L"Capsule");
    }

    bool CollisionWorld::Remove(ColliderId id)
    {
        const auto it=std::find_if(colliders_.begin(),colliders_.end(),[id](const ColliderComponent& c){return c.Id==id;});
        if(it==colliders_.end())return false;
        colliders_.erase(it);
        collidingIds_.erase(id);
        contacts_.erase(std::remove_if(contacts_.begin(),contacts_.end(),[id](const CollisionResult& r)
        {return r.ColliderA==id||r.ColliderB==id;}),contacts_.end());
        return true;
    }

    void CollisionWorld::Clear()
    {
        colliders_.clear();
        contacts_.clear();
        collidingIds_.clear();
        // ID는 재사용하지 않는다. GPU Readback이 한두 Frame 늦게 도착해도
        // 삭제된 ID가 새 Collider를 잘못 선택하는 상황을 막기 위함이다.
    }

    ColliderComponent* CollisionWorld::Find(ColliderId id)
    {
        const auto it=std::find_if(colliders_.begin(),colliders_.end(),[id](const ColliderComponent& c){return c.Id==id;});
        return it==colliders_.end()?nullptr:&*it;
    }
    const ColliderComponent* CollisionWorld::Find(ColliderId id)const
    {
        const auto it=std::find_if(colliders_.begin(),colliders_.end(),[id](const ColliderComponent& c){return c.Id==id;});
        return it==colliders_.end()?nullptr:&*it;
    }

    void CollisionWorld::UpdateCollisions()
    {
        contacts_.clear();
        collidingIds_.clear();
        const auto pairs=broadPhase_.BuildPairs(colliders_);
        contacts_.reserve(pairs.size());
        for(const auto& [aIndex,bIndex]:pairs)
        {
            CollisionResult result=CollisionDetector::Test(colliders_[aIndex],colliders_[bIndex]);
            if(!result.Hit)continue;
            collidingIds_.insert(result.ColliderA);
            collidingIds_.insert(result.ColliderB);
            contacts_.push_back(result);
        }
    }

    bool CollisionWorld::IsColliding(ColliderId id)const
    {
        return collidingIds_.find(id)!=collidingIds_.end();
    }

    std::vector<ColliderId> CollisionWorld::OverlapAABB(const AABB& bounds,LayerMask mask)const
    {
        std::vector<ColliderId> result;
        for(const auto& collider:colliders_)
        {
            if(!collider.Enabled||(collider.Layer&mask)==0u)continue;
            if(Overlaps(bounds,ComputeWorldBounds(collider)))result.push_back(collider.Id);
        }
        return result;
    }
}
