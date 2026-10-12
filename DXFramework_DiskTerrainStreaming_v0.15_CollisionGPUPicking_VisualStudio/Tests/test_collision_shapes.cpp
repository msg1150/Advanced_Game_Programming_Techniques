// ============================================================================
// test_collision_shapes.cpp
// ----------------------------------------------------------------------------
// Renderer/DirectX 없이 Narrow Phase 기본 조합을 검증하는 Portable Test.
// Windows 전용 D3D11 Build를 대체하는 테스트가 아니라 Physics Core 회귀 검사용이다.
// ============================================================================
#include "Physics/Collision/Detection/CollisionDetector.h"

#include <cassert>
#include <iostream>

using namespace Collision;

int main()
{
    ColliderComponent sphereA{1,L"SphereA",{0,0,0},{},SphereShape{2.f}};
    ColliderComponent sphereB{2,L"SphereB",{3,0,0},{},SphereShape{2.f}};
    assert(CollisionDetector::Test(sphereA,sphereB).Hit);
    sphereB.Position={10,0,0};
    assert(!CollisionDetector::Test(sphereA,sphereB).Hit);

    ColliderComponent aabb{3,L"AABB",{2.5f,0,0},{},AABBShape{{1,1,1}}};
    assert(CollisionDetector::Test(sphereA,aabb).Hit);

    ColliderComponent obb{4,L"OBB",{0,0,0},{0,DegreesToRadians(45.f),0},OBBShape{{2,1,1}}};
    ColliderComponent aabb2{5,L"AABB2",{2,0,0},{},AABBShape{{1,1,1}}};
    assert(CollisionDetector::Test(obb,aabb2).Hit);

    ColliderComponent capA{6,L"CapA",{0,0,0},{},CapsuleShape{1.f,2.f}};
    ColliderComponent capB{7,L"CapB",{1.5f,0,0},{},CapsuleShape{1.f,2.f}};
    assert(CollisionDetector::Test(capA,capB).Hit);
    capB.Position={8,0,0};
    assert(!CollisionDetector::Test(capA,capB).Hit);

    ColliderComponent obbFar{8,L"OBBFar",{20,0,0},{0,DegreesToRadians(30.f),0},OBBShape{{2,2,2}}};
    assert(!CollisionDetector::Test(capA,obbFar).Hit);

    std::cout << "collision shape tests passed\n";
    return 0;
}
