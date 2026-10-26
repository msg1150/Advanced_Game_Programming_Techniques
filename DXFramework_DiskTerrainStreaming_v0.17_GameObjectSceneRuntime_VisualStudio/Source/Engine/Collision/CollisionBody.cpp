#include "Engine/Collision/CollisionBody.h"
#include "Physics/Collision/Detection/CollisionDetector.h"
#include <set>

namespace Engine
{
Properties CollisionBody::Save()const
{
    Properties p{{"layer",std::to_string(Layer)},{"mask",std::to_string(Mask)},{"trigger",Trigger?"1":"0"}};
    const char* names[]={"Sphere","AABB","OBB","Capsule"};p["shape"]=names[Shape.index()];
    std::visit([&](const auto& s)
    {
        using T=std::decay_t<decltype(s)>;
        if constexpr(std::is_same_v<T,Collision::SphereShape>||std::is_same_v<T,Collision::CapsuleShape>)p["radius"]=Text(s.Radius);
        else Put(p,"extent",s.HalfExtents);
        if constexpr(std::is_same_v<T,Collision::CapsuleShape>)p["halfSegment"]=Text(s.HalfSegment);
    },Shape);return p;
}
void CollisionBody::Load(const Properties& p)
{
    auto bits=[&](const char* key,Collision::LayerMask fallback)
    {const auto v=Value(p,key,std::to_string(fallback));Collision::LayerMask result=0;const auto r=std::from_chars(v.data(),v.data()+v.size(),result);if(r.ec!=std::errc{}||r.ptr!=v.data()+v.size())throw std::runtime_error("Invalid collision bit mask");return result;};
    Layer=bits("layer",Collision::LayerDefault);Mask=bits("mask",Collision::LayerAll);
    const auto trigger=Value(p,"trigger","0");if(trigger!="0"&&trigger!="1")throw std::runtime_error("Invalid trigger flag");Trigger=trigger=="1";
    const auto shape=Value(p,"shape","OBB");const float radius=Number(p,"radius",.5f),half=Number(p,"halfSegment",.5f);
    const auto extent=Vector(p,"extent",{.5f,.5f,.5f});
    if(radius<.01f||half<0||extent.x<.01f||extent.y<.01f||extent.z<.01f)throw std::runtime_error("Invalid collider dimensions");
    if(shape=="Sphere")Shape=Collision::SphereShape{radius};else if(shape=="AABB")Shape=Collision::AABBShape{extent};
    else if(shape=="OBB")Shape=Collision::OBBShape{extent};else if(shape=="Capsule")Shape=Collision::CapsuleShape{radius,half};
    else throw std::runtime_error("Unknown collision shape");
}
Collision::ColliderComponent CollisionBody::Proxy()const
{
    using namespace Collision;
    ColliderComponent p;p.Position=Owner().WorldMatrix().T;p.Layer=Layer;p.Mask=Mask;p.IsTrigger=Trigger;p.Enabled=Owner().Active()&&Enabled();
    const auto m=Owner().WorldMatrix();const Vec3 scale{Length(m.X),Length(m.Y),Length(m.Z)};
    const Vec3 x=Normalize(m.X),y=Normalize(m.Y),z=Normalize(m.Z);
    const bool shear=std::abs(Dot(x,y))>.0001f||std::abs(Dot(y,z))>.0001f||std::abs(Dot(x,z))>.0001f;
    const float maximum=shear?std::sqrt(LengthSq(m.X)+LengthSq(m.Y)+LengthSq(m.Z)):std::max({scale.x,scale.y,scale.z});
    p.Rotation.y=std::asin(std::clamp(-x.z,-1.f,1.f));
    if(std::abs(std::cos(p.Rotation.y))>1.e-5f){p.Rotation.x=std::atan2(y.z,z.z);p.Rotation.z=std::atan2(x.y,x.x);}
    else {p.Rotation.x=std::atan2(-z.y,y.y);p.Rotation.z=0;}
    std::visit([&](const auto& s)
    {
        using T=std::decay_t<decltype(s)>;
        if constexpr(std::is_same_v<T,SphereShape>)p.Shape=SphereShape{s.Radius*maximum};
        else if constexpr(std::is_same_v<T,CapsuleShape>)p.Shape=CapsuleShape{s.Radius*maximum,s.HalfSegment*scale.y};
        else
        {
            if(shear||std::is_same_v<T,AABBShape>)p.Shape=AABBShape{Abs(m.X)*s.HalfExtents.x+Abs(m.Y)*s.HalfExtents.y+Abs(m.Z)*s.HalfExtents.z};
            else p.Shape=OBBShape{{s.HalfExtents.x*scale.x,s.HalfExtents.y*scale.y,s.HalfExtents.z*scale.z}};
        }
    },Shape);return p;
}
void RegisterCollisionComponents(ComponentRegistry& registry){registry.Register("Collider",[]{return std::make_unique<CollisionBody>();});}
void CollisionBridge::Reset(){world_.Clear();bodies_.clear();pairs_.clear();sceneToken_=0;}
void CollisionBridge::Synchronize(Scene& scene)
{
    if(sceneToken_!=scene.Token()){Reset();sceneToken_=scene.Token();}
    std::set<ObjectId> live;
    for(auto* object:scene.Objects())if(auto* body=object->Get<CollisionBody>())
    {
        live.insert(object->Id());auto it=bodies_.find(object->Id());
        if(it==bodies_.end())it=bodies_.emplace(object->Id(),world_.AddSphere({},1)).first;
        auto proxy=body->Proxy();proxy.Id=it->second;*world_.Find(it->second)=std::move(proxy);
    }
    for(auto it=bodies_.begin();it!=bodies_.end();)if(!live.contains(it->first)){world_.Remove(it->second);it=bodies_.erase(it);}else ++it;
}
ObjectId CollisionBridge::ObjectForCollider(Collision::ColliderId id)const
{for(const auto& [object,collider]:bodies_)if(collider==id)return object;return 0;}
void CollisionBridge::DetectAndDispatch(Scene& scene)
{
    Synchronize(scene);world_.UpdateCollisions();decltype(pairs_) current;
    for(const auto& hit:world_.Contacts())
    {
        auto a=ObjectForCollider(hit.ColliderA),b=ObjectForCollider(hit.ColliderB);auto normal=hit.Normal;
        if(a>b){std::swap(a,b);normal=-normal;}
        current[{a,b}]={hit.TriggerPair,hit.Point,normal,hit.PenetrationDepth};
    }
    auto send=[&](const auto& pair,const PairData& data,ContactPhase phase)
    {
        scene.Notify(pair.first,{{scene.Token(),pair.second},phase,data.Trigger,data.Point,data.Normal,data.Depth});
        scene.Notify(pair.second,{{scene.Token(),pair.first},phase,data.Trigger,data.Point,-data.Normal,data.Depth});
    };
    for(const auto& [key,data]:current)send(key,data,pairs_.contains(key)?ContactPhase::Stay:ContactPhase::Enter);
    for(const auto& [key,data]:pairs_)if(!current.contains(key))send(key,data,ContactPhase::Exit);
    pairs_=std::move(current);
}
void CollisionBridge::Move(GameObject& object,Vec3 displacement,const GroundQuery& ground,float clearance)
{
    using namespace Collision;
    Synchronize(object.World());auto* body=object.Get<CollisionBody>();
    const auto own=body?bodies_.at(object.Id()):0;
    for(unsigned axis=0;axis<3;++axis)
    {
        const Vec3 original=object.WorldMatrix().T;Vec3 next=original;
        if(axis==0)next.x+=displacement.x;else if(axis==1)next.y+=displacement.y;else next.z+=displacement.z;
        object.SetWorldPosition(next);
        if(ground)
        {
            float height=0;Vec3 normal;
            if(!ground(next.x,next.z,height,normal)){object.SetWorldPosition(original);continue;}
            const float bottom=body?ComputeWorldBounds(body->Proxy()).Min.y:next.y;
            next.y+=height+clearance-bottom;object.SetWorldPosition(next);
        }
        bool blocked=false;
        if(body&&body->Enabled()&&!body->Trigger)
        {
            auto candidate=body->Proxy();candidate.Id=own;
            for(const auto& other:world_.Colliders())
            {
                if(other.Id==own||!other.Enabled||other.IsTrigger||(candidate.Mask&other.Layer)==0||(other.Mask&candidate.Layer)==0)continue;
                const auto hit=CollisionDetector::Test(candidate,other);
                if(hit.Hit&&hit.PenetrationDepth>.001f){blocked=true;break;}
            }
        }
        if(blocked)object.SetWorldPosition(original);
        else if(body){auto proxy=body->Proxy();proxy.Id=own;*world_.Find(own)=proxy;}
    }
}
}
