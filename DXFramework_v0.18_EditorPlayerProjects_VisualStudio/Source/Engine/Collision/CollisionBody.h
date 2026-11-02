#pragma once
#include "Engine/Core/Scene.h"
#include "Physics/Collision/Core/CollisionWorld.h"
#include <map>

namespace Engine
{
class CollisionBody final:public Component
{
public:
    Collision::ShapeData Shape=Collision::OBBShape{{.5f,.5f,.5f}};
    Collision::LayerMask Layer=Collision::LayerDefault,Mask=Collision::LayerAll;
    bool Trigger=false;
    unsigned Enters=0,Stays=0,Exits=0;
    std::string Type()const override{return "Collider";}
    Properties Save()const override;
    void Load(const Properties&)override;
    Collision::ColliderComponent Proxy()const;
protected:
    void OnContact(const ContactEvent& e)override
    {if(e.Phase==ContactPhase::Enter)++Enters;else if(e.Phase==ContactPhase::Stay)++Stays;else ++Exits;}
};
void RegisterCollisionComponents(ComponentRegistry& registry);
using GroundQuery=std::function<bool(float x,float z,float& height,Vec3& normal)>;
class CollisionBridge
{
public:
    void Reset();
    void Synchronize(Scene& scene);
    void DetectAndDispatch(Scene& scene);
    void Move(GameObject& object,Vec3 displacement,const GroundQuery& ground={},float clearance=.02f);
    ObjectId ObjectForCollider(Collision::ColliderId id)const;
    Collision::CollisionWorld& World(){return world_;}
private:
    Collision::CollisionWorld world_;
    std::uint64_t sceneToken_=0;
    std::map<ObjectId,Collision::ColliderId> bodies_;
    struct PairData{bool Trigger;Vec3 Point,Normal;float Depth;};
    std::map<std::pair<ObjectId,ObjectId>,PairData> pairs_;
};
}
