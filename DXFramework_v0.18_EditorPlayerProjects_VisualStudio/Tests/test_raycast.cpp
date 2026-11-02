// ============================================================================
// test_raycast.cpp
// ----------------------------------------------------------------------------
// CPU Scene Query Raycast가 Sphere/AABB/OBB/Capsule에 동작하는지 검증한다.
// ============================================================================
#include "Physics/Query/Raycast.h"

#include <cassert>
#include <iostream>
#include <vector>

using namespace Collision;

int main()
{
    Ray ray{{-10,0,0},{1,0,0}};

    ColliderComponent sphere{1,L"Sphere",{0,0,0},{},SphereShape{2.f}};
    assert(Raycast::Test(ray,sphere).Hit);

    ColliderComponent aabb{2,L"AABB",{5,0,0},{},AABBShape{{1,1,1}}};
    assert(Raycast::Test(ray,aabb).Hit);

    ColliderComponent obb{3,L"OBB",{10,0,0},{0,DegreesToRadians(35.f),0},OBBShape{{2,1,1}}};
    assert(Raycast::Test(ray,obb).Hit);

    ColliderComponent capsule{4,L"Capsule",{15,0,0},{0,0,DegreesToRadians(90.f)},CapsuleShape{1.f,2.f}};
    assert(Raycast::Test(ray,capsule).Hit);

    std::vector<ColliderComponent> scene={sphere,aabb,obb,capsule};
    const RaycastHit closest=Raycast::Closest(ray,scene);
    assert(closest.Hit&&closest.Collider==1u);

    scene[0].Layer=LayerEditor;
    const RaycastHit filtered=Raycast::Closest(ray,scene,LayerDefault);
    assert(filtered.Hit&&filtered.Collider==2u);

    std::cout << "raycast tests passed\n";
    return 0;
}
