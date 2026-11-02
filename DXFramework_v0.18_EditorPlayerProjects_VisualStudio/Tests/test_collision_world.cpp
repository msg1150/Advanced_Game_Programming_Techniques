// ============================================================================
// test_collision_world.cpp
// ----------------------------------------------------------------------------
// Broad Phase, Layer/Mask, Trigger, ID 재사용 금지를 검증한다.
// ============================================================================
#include "Physics/Collision/Core/CollisionWorld.h"

#include <cassert>
#include <iostream>

using namespace Collision;

int main()
{
    CollisionWorld world;
    const ColliderId a=world.AddSphere({0,0,0},2.f);
    const ColliderId b=world.AddAABB({2,0,0},{1,1,1});
    world.UpdateCollisions();
    assert(world.Contacts().size()==1u);

    auto* ca=world.Find(a);
    auto* cb=world.Find(b);
    assert(ca&&cb);
    ca->Layer=1u<<5u;
    ca->Mask=1u<<6u;
    cb->Layer=1u<<6u;
    cb->Mask=1u<<5u;
    cb->IsTrigger=true;
    world.UpdateCollisions();
    assert(world.Contacts().size()==1u);
    assert(world.Contacts().front().TriggerPair);

    cb->Mask=0u;
    world.UpdateCollisions();
    assert(world.Contacts().empty());

    const ColliderId old=b;
    assert(world.Remove(b));
    const ColliderId next=world.AddCapsule({0,0,0},1.f,2.f);
    assert(next>old); // Late GPU Readback 안전성: 삭제 ID 재사용 금지.

    const auto overlaps=world.OverlapAABB({{-3,-3,-3},{3,3,3}});
    assert(!overlaps.empty());

    std::cout << "collision world tests passed\n";
    return 0;
}
