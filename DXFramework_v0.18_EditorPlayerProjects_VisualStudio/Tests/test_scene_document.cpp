#include "Scene/Runtime/ColliderSceneAdapter.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <sstream>

int main()
{
    using namespace Collision;
    CollisionWorld world;
    const auto first=world.AddSphere({1.25f,3,-2},2.5f);
    world.AddAABB({0,0,0},{1,2,3});world.AddOBB({4,5,6},{2,3,4});world.AddCapsule({7,8,9},1.5f,4);
    auto* c=world.Find(first);c->Name=L"한글 \\\"이름\"\n🌍";c->Layer=0x80000000u;c->Mask=0;c->Enabled=false;c->IsTrigger=true;
    world.Colliders()[2].Rotation={.1f,.2f,.3f};
    auto document=ColliderSceneAdapter::Capture(world);std::wstring error;
    std::ostringstream out;assert(ColliderSceneSerializer::Write(out,document,error));
    std::istringstream in(out.str());ColliderSceneDocument loaded;
    assert(ColliderSceneSerializer::Read(in,loaded,error));
    assert(loaded.Colliders.size()==4&&loaded.Colliders[0].Id==0);
    assert(loaded.Colliders[0].Name==c->Name&&loaded.Colliders[0].Mask==0&&loaded.Colliders[0].Layer==0x80000000u);
    assert(!loaded.Colliders[0].Enabled&&loaded.Colliders[0].IsTrigger);
    assert(loaded.Colliders[2].Rotation.y==.2f);
    for(std::size_t i=0;i<4;++i)assert(loaded.Colliders[i].Shape.index()==i);
    const auto oldMax=world.Colliders().back().Id;
    ColliderSceneAdapter::Apply(loaded,world);
    assert(world.Count()==4&&world.Colliders().front().Id>oldMax&&!world.Find(first));
    const auto nextMax=world.Colliders().back().Id;
    ColliderSceneAdapter::Apply(loaded,world);assert(world.Colliders().front().Id>nextMax);
    // A failed parse never changes the document/current runtime scene.
    const auto preserved=loaded.Colliders[0].Name;
    for(const char* invalid:{"DXCOLLIDER_SCENE 2 0","DXCOLLIDER_SCENE 1 100001","DXCOLLIDER_SCENE 1 1 Sphere",
        "DXCOLLIDER_SCENE 1 0 trailing","DXCOLLIDER_SCENE 1 1 Sphere \"x\" 0 0 0 0 0 0 1 1 2 0 1",
        "DXCOLLIDER_SCENE 1 1 Sphere \"x\" 0 0 0 0 0 0 1 1 1 0 -2",
        "DXCOLLIDER_SCENE 1 1 Unknown \"x\" 0 0 0 0 0 0 1 1 1 0"})
    {
        std::istringstream bad(invalid);assert(!ColliderSceneSerializer::Read(bad,loaded,error));
        assert(!error.empty()&&loaded.Colliders[0].Name==preserved&&world.Count()==4);
    }
    document.Colliders[0].Position.x=std::numeric_limits<float>::infinity();std::ostringstream invalid;
    assert(!ColliderSceneSerializer::Write(invalid,document,error)&&invalid.str().empty());
    std::istringstream empty("DXCOLLIDER_SCENE 1 0");assert(ColliderSceneSerializer::Read(empty,loaded,error));
    ColliderSceneAdapter::Apply(loaded,world);assert(world.Count()==0);
}
