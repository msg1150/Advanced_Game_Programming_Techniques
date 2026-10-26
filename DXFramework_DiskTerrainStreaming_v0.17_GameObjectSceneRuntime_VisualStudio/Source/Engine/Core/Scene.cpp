#include "Engine/Core/Scene.h"
#include <algorithm>
#include <limits>

namespace Engine
{
bool ComponentRegistry::Register(std::string type,Factory factory)
{if(type.empty()||!factory)return false;return factories_.emplace(std::move(type),std::move(factory)).second;}
std::unique_ptr<Component> ComponentRegistry::Create(const std::string& type,bool* missing)const
{
    const auto it=factories_.find(type);if(missing)*missing=it==factories_.end();
    auto result=it==factories_.end()?std::make_unique<MissingComponent>(type):it->second();
    if(!result||result->Type()!=type)throw std::runtime_error("Component factory type mismatch");return result;
}
ObjectHandle GameObject::Handle()const{return {scene_->Token(),id_};}
bool GameObject::SetParent(ObjectId parent)
{
    ObjectId cursor=parent;unsigned depth=0;
    while(cursor){if(cursor==id_||++depth>128)return false;const auto* p=scene_->Find(cursor);if(!p)return false;cursor=p->parent_;}
    // Retains LOCAL TRS. No lossy decomposition of sheared matrices.
    parent_=parent;return true;
}
bool GameObject::Active()const
{
    if(destroyed_||!enabled_)return false;
    if(parent_){const auto* p=scene_->Find(parent_);return p&&p->Active();}return true;
}
Matrix GameObject::WorldMatrix()const
{
    if(parent_)if(const auto* p=scene_->Find(parent_))return p->WorldMatrix()*Local.LocalMatrix();
    return Local.LocalMatrix();
}
void GameObject::SetWorldPosition(Vec3 p)
{if(parent_)if(const auto* parent=scene_->Find(parent_)){Local.Position=parent->WorldMatrix().InversePoint(p);return;}Local.Position=p;}
Component& GameObject::Add(std::unique_ptr<Component> c)
{
    if(!c)throw std::invalid_argument("Null component");
    for(const auto& existing:components_)if(!existing->removed_&&existing->Type()==c->Type())throw std::runtime_error("Duplicate component type");
    c->owner_=this;components_.push_back(std::move(c));return *components_.back();
}
bool GameObject::Remove(const std::string& type)
{for(auto& c:components_)if(!c->removed_&&c->Type()==type){c->removed_=true;return true;}return false;}
std::vector<Component*> GameObject::Components()const
{std::vector<Component*> result;for(const auto& c:components_)if(!c->removed_)result.push_back(c.get());return result;}
Scene::Scene(std::string name,SceneTiming timing):name_(std::move(name)),timing_(timing)
{
    static std::atomic<std::uint64_t> sequence{1};token_=sequence.fetch_add(1);
    if(!std::isfinite(timing.FixedDelta)||timing.FixedDelta<=0||!std::isfinite(timing.MaxFrameDelta)||timing.MaxFrameDelta<timing.FixedDelta||!timing.MaxSteps)
        throw std::invalid_argument("Invalid scene timing");
}
Scene::~Scene(){Exit();}
GameObject& Scene::Create(std::string name,ObjectId requested)
{
    const ObjectId id=requested?requested:nextId_;
    if(!id||id==std::numeric_limits<ObjectId>::max()||usedIds_.contains(id))throw std::runtime_error("Invalid/duplicate object ID");
    auto object=std::unique_ptr<GameObject>(new GameObject(*this,id,std::move(name)));
    usedIds_.insert(id);nextId_=std::max(nextId_,id+1);objects_.push_back(std::move(object));return *objects_.back();
}
GameObject* Scene::Find(ObjectId id)const
{for(const auto& object:objects_)if(object->id_==id&&!object->destroyed_)return object.get();return nullptr;}
std::vector<GameObject*> Scene::Objects()const
{std::vector<GameObject*> result;for(const auto& o:objects_)if(!o->destroyed_)result.push_back(o.get());return result;}
bool Scene::Destroy(ObjectId id)
{
    auto* object=Find(id);if(!object)return false;
    for(auto* child:Objects())if(child->parent_==id)Destroy(child->id_);
    object->destroyed_=true;return true;
}
void Scene::Detach(Component& c)
{
    // Clear flags before callbacks to make reentrant removal idempotent.
    if(c.running_){c.running_=false;try{c.OnStop();}catch(...){error_="Component OnStop failed";}}
    if(c.attached_){c.attached_=false;try{c.OnDetach();}catch(...){error_="Component OnDetach failed";}}
}
void Scene::Flush()
{
    if(dispatchDepth_)return;
    // Callbacks may create objects: use stable heap-object snapshots, never live iterators.
    ++dispatchDepth_;
    for(unsigned pass=0;pass<1024;++pass)
    {
        std::vector<Component*> pending;
        for(const auto& object:objects_)for(const auto& c:object->components_)
            if((object->destroyed_||c->removed_)&&(c->attached_||c->running_))pending.push_back(c.get());
        if(pending.empty())break;
        for(auto* c:pending)Detach(*c);
    }
    --dispatchDepth_;
    for(auto& o:objects_)o->components_.erase(std::remove_if(o->components_.begin(),o->components_.end(),[](const auto& c){return c->removed_&&!c->attached_;}),o->components_.end());
    objects_.erase(std::remove_if(objects_.begin(),objects_.end(),[](const auto& o){return o->destroyed_;}),objects_.end());
}
void Scene::Dispatch(Phase phase,float dt,ObjectId target,const ContactEvent* event)
{
    ++dispatchDepth_;
    const auto objects=Objects();
    for(auto* object:objects)
    {
        if(target&&object->Id()!=target)continue;
        const auto components=object->Components();
        for(auto* c:components)
        {
            if(object->destroyed_||c->removed_)continue;
            try
            {
                if(phase==Phase::Reconcile)
                {
                    if(!c->attached_){c->attached_=true;c->OnAttach();}
                    if(c->removed_||object->destroyed_)continue;
                    const bool active=object->Active()&&c->enabled_&&!c->failed_;
                    if(active&&!c->running_){c->running_=true;c->OnStart();}
                    else if(!active&&c->running_){c->running_=false;c->OnStop();}
                }
                else if(c->running_&&c->enabled_&&object->Active())
                {
                    if(phase==Phase::Fixed)c->OnFixedUpdate(dt);
                    else if(phase==Phase::Update)c->OnUpdate(dt);
                    else if(event)c->OnContact(*event);
                }
            }
            catch(const std::exception& e){c->failed_=true;c->enabled_=false;error_=c->Type()+": "+e.what();}
            catch(...){c->failed_=true;c->enabled_=false;error_=c->Type()+": unknown component failure";}
        }
    }
    --dispatchDepth_;Flush();
}
void Scene::Enter(){if(entered_)return;entered_=true;accumulator_=0;Dispatch(Phase::Reconcile);}
void Scene::Exit()
{
    if(!entered_)return;entered_=false;++dispatchDepth_;
    const auto objects=Objects();for(auto* o:objects)for(auto* c:o->Components())Detach(*c);
    --dispatchDepth_;Flush();accumulator_=0;
}
void Scene::Refresh(){if(entered_)Dispatch(Phase::Reconcile);else Flush();}
void Scene::Tick(float dt)
{
    if(!entered_)Enter();
    if(!std::isfinite(dt)||dt<0)return;
    const float frame=std::min(dt,timing_.MaxFrameDelta);Dispatch(Phase::Reconcile);accumulator_+=frame;
    unsigned steps=0;
    while(accumulator_+1.e-7f>=timing_.FixedDelta&&steps++<timing_.MaxSteps)
    {Dispatch(Phase::Fixed,timing_.FixedDelta);if(AfterFixedUpdate)AfterFixedUpdate(*this,timing_.FixedDelta);accumulator_-=timing_.FixedDelta;}
    if(accumulator_>=timing_.FixedDelta)accumulator_=std::fmod(accumulator_,timing_.FixedDelta);
    Dispatch(Phase::Update,frame);
}
void Scene::Notify(ObjectId id,const ContactEvent& event){if(entered_)Dispatch(Phase::Contact,0,id,&event);}
bool SceneManager::Request(const std::string& key)
{if(shuttingDown_)return false;const auto it=factories_.find(key);if(it==factories_.end())return false;auto next=it->second();if(!next)return false;pending_=std::move(next);return true;}
void SceneManager::Update(float dt)
{
    if(updating_||shuttingDown_)return;
    struct Guard{bool& Flag;explicit Guard(bool& f):Flag(f){Flag=true;}~Guard(){Flag=false;}} guard(updating_);
    if(stopRequested_){pending_.reset();current_.reset();stopRequested_=false;return;}
    if(pending_){auto next=std::move(pending_);if(current_)current_->Exit();current_=std::move(next);current_->Enter();}
    if(current_)current_->Tick(dt);
}
}
