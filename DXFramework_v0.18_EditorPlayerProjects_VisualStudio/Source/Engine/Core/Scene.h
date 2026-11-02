#pragma once
#include "Engine/Core/Properties.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace Engine
{
using ObjectId=std::uint64_t;
class Scene;class GameObject;
struct ObjectHandle
{
    std::uint64_t SceneToken=0;ObjectId Id=0;
    bool operator==(const ObjectHandle&)const=default;
};
enum class ContactPhase{Enter,Stay,Exit};
struct ContactEvent{ObjectHandle Other;ContactPhase Phase;bool Trigger=false;Vec3 Point{},Normal{};float Depth=0;};
class Component
{
public:
    virtual ~Component()=default;
    virtual std::string Type()const=0;
    virtual Properties Save()const{return {};}
    virtual void Load(const Properties&){}
    virtual std::vector<ObjectId> References()const{return {};}
    virtual void RemapReferences(const std::unordered_map<ObjectId,ObjectId>& remap)
    {for(auto id:References())if(remap.contains(id))throw std::runtime_error("Component must implement reference remapping: "+Type());}
    GameObject& Owner()const{return *owner_;}
    bool Enabled()const{return enabled_;}
    void SetEnabled(bool enabled){enabled_=enabled;}
    bool Running()const{return running_&&!removed_;}
protected:
    virtual void OnAttach(){}
    virtual void OnStart(){}
    virtual void OnFixedUpdate(float){}
    virtual void OnUpdate(float){}
    virtual void OnStop(){}
    virtual void OnDetach(){}
    virtual void OnContact(const ContactEvent&){}
private:
    friend class Scene;friend class GameObject;
    GameObject* owner_=nullptr;
    bool enabled_=true,attached_=false,running_=false,removed_=false,failed_=false;
};
class MissingComponent final:public Component
{
public:
    explicit MissingComponent(std::string type):type_(std::move(type)){}
    std::string Type()const override{return type_;}
    Properties Save()const override{return properties_;}
    void Load(const Properties& p)override{properties_=p;}
private:
    std::string type_;Properties properties_;
};
class ComponentRegistry
{
public:
    using Factory=std::function<std::unique_ptr<Component>()>;
    bool Register(std::string type,Factory factory);
    std::unique_ptr<Component> Create(const std::string& type,bool* missing=nullptr)const;
private:
    std::unordered_map<std::string,Factory> factories_;
};
class GameObject
{
public:
    ObjectId Id()const{return id_;}
    ObjectHandle Handle()const;
    Scene& World()const{return *scene_;}
    std::string Name;
    Transform Local;
    ObjectId Parent()const{return parent_;}
    bool SetParent(ObjectId parent);
    bool Enabled()const{return enabled_;}
    void SetEnabled(bool value){enabled_=value;}
    bool Active()const;
    Matrix WorldMatrix()const;
    void SetWorldPosition(Vec3 position);
    Component& Add(std::unique_ptr<Component> component);
    bool Remove(const std::string& type);
    template<class T>T* Get()const
    {for(const auto& c:components_)if(!c->removed_)if(auto* value=dynamic_cast<T*>(c.get()))return value;return nullptr;}
    std::vector<Component*> Components()const;
private:
    friend class Scene;
    GameObject(Scene& scene,ObjectId id,std::string name):Name(std::move(name)),scene_(&scene),id_(id){}
    Scene* scene_;ObjectId id_,parent_=0;bool enabled_=true,destroyed_=false;
    std::vector<std::unique_ptr<Component>> components_;
};
struct SceneTiming{float FixedDelta=1.f/60.f,MaxFrameDelta=.25f;unsigned MaxSteps=8;};
class Scene
{
public:
    explicit Scene(std::string name={},SceneTiming timing={});
    ~Scene();
    Scene(const Scene&)=delete;Scene& operator=(const Scene&)=delete;
    GameObject& Create(std::string name,ObjectId requestedId=0);
    GameObject* Find(ObjectId id)const;
    GameObject* Resolve(ObjectHandle handle)const{return handle.SceneToken==token_?Find(handle.Id):nullptr;}
    bool Destroy(ObjectId id);
    std::vector<GameObject*> Objects()const;
    std::uint64_t Token()const{return token_;}
    const std::string& Name()const{return name_;}
    void Enter();void Exit();void Tick(float dt);void Refresh();
    bool Entered()const{return entered_;}
    void Notify(ObjectId id,const ContactEvent& event);
    std::function<void(Scene&,float)> AfterFixedUpdate;
    const std::string& LastError()const{return error_;}
private:
    friend class GameObject;
    enum class Phase{Reconcile,Fixed,Update,Contact};
    void Dispatch(Phase phase,float dt=0,ObjectId target=0,const ContactEvent* event=nullptr);
    void Flush();void Detach(Component& component);
    std::string name_,error_;SceneTiming timing_;std::uint64_t token_;ObjectId nextId_=1;
    std::vector<std::unique_ptr<GameObject>> objects_;
    std::unordered_set<ObjectId> usedIds_;
    bool entered_=false;unsigned dispatchDepth_=0;float accumulator_=0;
};
class SceneManager
{
public:
    ~SceneManager(){shuttingDown_=true;current_.reset();pending_.reset();}
    using Factory=std::function<std::unique_ptr<Scene>()>;
    bool Register(std::string key,Factory factory){return factories_.emplace(std::move(key),std::move(factory)).second;}
    bool Request(const std::string& key);
    void Request(std::unique_ptr<Scene> scene){if(!shuttingDown_)pending_=std::move(scene);}
    void Update(float dt);
    Scene* Current()const{return current_.get();}
    void Stop(){stopRequested_=true;}
private:
    std::unordered_map<std::string,Factory> factories_;
    std::unique_ptr<Scene> current_,pending_;
    bool stopRequested_=false,shuttingDown_=false,updating_=false;
};
}
