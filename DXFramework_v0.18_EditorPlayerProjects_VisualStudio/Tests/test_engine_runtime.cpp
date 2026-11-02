#include "Engine/Core/Scene.h"
#include "Engine/Serialization/SceneSerializer.h"
#include "Engine/Components/ObjectLink.h"
#include "Engine/Collision/CollisionBody.h"
#include "Engine/Gameplay/Movement.h"
#include "Engine/Graphics/MeshRendererComponent.h"
#include "Engine/Resources/ResourceCache.h"
#include <cassert>
#include <sstream>
#include <fstream>

using namespace Engine;
struct Counts{int Attach=0,Start=0,Fixed=0,Update=0,Stop=0,Detach=0;};
class Probe:public Component
{
public:
    explicit Probe(Counts& c):counts(c){}
    std::string Type()const override{return "Probe";}
    bool RemoveSelf=false;
protected:
    void OnAttach()override{++counts.Attach;}void OnStart()override{++counts.Start;}
    void OnFixedUpdate(float)override{++counts.Fixed;}
    void OnUpdate(float)override{++counts.Update;if(RemoveSelf)Owner().Remove(Type());}
    void OnStop()override{++counts.Stop;}void OnDetach()override{++counts.Detach;}
private:Counts& counts;
};
class RemoveOnAttach final:public Component
{
public:
    int& Starts;
    explicit RemoveOnAttach(int& starts):Starts(starts){}
    std::string Type()const override{return "RemoveOnAttach";}
protected:
    void OnAttach()override{Owner().Remove(Type());}
    void OnStart()override{++Starts;}
};
static bool Near(float a,float b){return std::abs(a-b)<.001f;}
int main()
{
    Counts counts;
    {
        Scene scene("Lifecycle");auto& object=scene.Create("Object");auto p=std::make_unique<Probe>(counts);auto* probe=p.get();object.Add(std::move(p));
        scene.Enter();scene.Tick(1.f/30);assert(counts.Attach==1&&counts.Start==1&&counts.Fixed==2&&counts.Update==1);
        object.SetEnabled(false);scene.Tick(.01f);assert(counts.Stop==1);
        object.SetEnabled(true);scene.Tick(.01f);assert(counts.Start==2);
        probe->RemoveSelf=true;scene.Tick(.01f);assert(!object.Get<Probe>()&&counts.Detach==1&&counts.Stop==2);
        auto& parent=scene.Create("Parent");parent.Local.Position={10,0,0};parent.Local.Scale={2,2,2};
        auto& child=scene.Create("Child");child.Local.Position={1,2,3};assert(child.SetParent(parent.Id()));
        assert(Near(child.WorldMatrix().T.x,12));child.SetWorldPosition({14,4,6});assert(Near(child.Local.Position.x,2));
        assert(!parent.SetParent(child.Id()));const auto handle=child.Handle();
        assert(scene.Destroy(parent.Id()));scene.Refresh();assert(!scene.Resolve(handle));
        bool reused=false;try{scene.Create("Reused",handle.Id);}catch(const std::exception&){reused=true;}assert(reused);
        int starts=0;object.Add(std::make_unique<RemoveOnAttach>(starts));scene.Refresh();
        assert(starts==0&&!object.Get<RemoveOnAttach>());
    }
    ComponentRegistry registry;RegisterCoreComponents(registry);RegisterCollisionComponents(registry);RegisterGraphicsComponents(registry);RegisterGameplayComponents(registry);
    Scene original("저장 테스트");auto& parent=original.Create("Parent",41);auto& child=original.Create("Child",99);assert(child.SetParent(parent.Id()));
    child.Local.Position={1,2,3};child.Local.Rotation={.1f,.2f,.3f};child.Local.Scale={2,3,4};
    auto link=std::make_unique<ObjectLink>();link->Target=parent.Id();child.Add(std::move(link));
    auto mesh=std::make_unique<MeshRendererComponent>();mesh->MeshAsset="Assets/Meshes/Cube.mesh";child.Add(std::move(mesh));
    auto body=std::make_unique<CollisionBody>();body->Trigger=true;body->Shape=Collision::CapsuleShape{.4f,1};child.Add(std::move(body));
    child.Add(std::make_unique<Movement>());
    std::stringstream file;std::string error;assert(SceneSerializer::Write(file,original,error));
    std::unique_ptr<Scene> loaded;assert(SceneSerializer::Read(file,registry,loaded,error));assert(loaded->Find(99)->Get<ObjectLink>()->Resolve()==loaded->Find(41));
    assert(loaded->Find(99)->Local.Scale.z==4&&loaded->Find(99)->Get<CollisionBody>()->Trigger);
    assert(!loaded->Resolve(child.Handle()));
    // Optional modules can be absent; their payload round-trips without factory calls.
    ComponentRegistry minimal;std::istringstream absent(file.str());std::unique_ptr<Scene> detached;std::vector<std::string> warnings;
    assert(SceneSerializer::Read(absent,minimal,detached,error,&warnings)&&warnings.size()==4);
    std::ostringstream roundtrip;assert(SceneSerializer::Write(roundtrip,*detached,error));std::istringstream restored(roundtrip.str());
    assert(SceneSerializer::Read(restored,registry,loaded,error)&&loaded->Find(99)->Get<MeshRendererComponent>());
    const auto preserved=loaded.get();
    for(const auto& invalid:{std::string("DXENGINE_SCENE 9 \"x\" 0"),std::string("DXENGINE_SCENE 1 \"x\" 1 OBJECT 1 1 1 \"x\" 0 0 0 0 0 0 1 1 1 0"),file.str()+" trailing"})
    {std::istringstream bad(invalid);assert(!SceneSerializer::Read(bad,registry,loaded,error)&&loaded.get()==preserved);}
    SceneManager manager;manager.Request(std::move(loaded));manager.Update(0);const auto oldToken=manager.Current()->Token();
    assert(manager.Register("empty",[]{return std::make_unique<Scene>("Empty");}));assert(manager.Request("empty"));
    assert(manager.Current()->Token()==oldToken);manager.Update(0);assert(manager.Current()->Token()!=oldToken);
    ResourceCache<int> cache;int loads=0;std::string resourceError;
    auto loader=[&](std::string&){++loads;return std::make_shared<int>(42);};auto a=cache.Acquire("same",loader,resourceError),b=cache.Acquire("same",loader,resourceError);
    assert(a==b&&loads==1);cache.CollectUnused();assert(cache.Size()==1);a.reset();b.reset();cache.CollectUnused();assert(cache.Size()==0);
    auto fail=[&](std::string& e)->std::shared_ptr<int>{++loads;e="missing";return {};};cache.Acquire("bad",fail,resourceError);cache.Acquire("bad",fail,resourceError);assert(loads==2);
    cache.Invalidate("bad");cache.Acquire("bad",fail,resourceError);assert(loads==3);
    // Fixed-step movement, collision response, and Enter/Stay/Exit propagation.
    Scene play("Play");auto& player=play.Create("Player");auto& wall=play.Create("Wall");wall.Local.Position={2,0,0};
    auto& playerBody=static_cast<CollisionBody&>(player.Add(std::make_unique<CollisionBody>()));wall.Add(std::make_unique<CollisionBody>());
    play.Enter();CollisionBridge bridge;bridge.Move(player,{2,0,0});assert(Near(player.WorldMatrix().T.x,0));
    auto* wallBody=wall.Get<CollisionBody>();wallBody->Trigger=true;
    bridge.Move(player,{2,0,0});bridge.DetectAndDispatch(play);assert(playerBody.Enters==1);
    bridge.DetectAndDispatch(play);assert(playerBody.Stays==1);wall.SetEnabled(false);bridge.DetectAndDispatch(play);assert(playerBody.Exits==1);
    bridge.Move(player,{0,0,1},[](float,float,float& height,Vec3& normal){height=7;normal={0,1,0};return true;});
    assert(Near(Collision::ComputeWorldBounds(playerBody.Proxy()).Min.y,7.02f));
    const auto before=player.WorldMatrix().T;bridge.Move(player,{1,0,0},[](float,float,float&,Vec3&){return false;});assert(player.WorldMatrix().T.x==before.x);
    auto& mover=static_cast<Movement&>(player.Add(std::make_unique<Movement>()));mover.InputDirection={1,0,0};mover.Speed=6;const auto x=player.WorldMatrix().T.x;
    play.Tick(1.f/30);assert(Near(player.WorldMatrix().T.x,x+.2f));
    std::ifstream sample("Assets/Scenes/Runtime.engine.scene");std::unique_ptr<Scene> sampleScene;
    assert(SceneSerializer::Read(sample,registry,sampleScene,error)&&sampleScene->Objects().size()==5);
    assert(sampleScene->Find(4)->Parent()==1&&sampleScene->Find(3)->Get<ObjectLink>()->Target==1);
    manager.Stop();manager.Update(0);assert(!manager.Current());
}
