#pragma once
#include "Engine/Core/Scene.h"
namespace Engine
{
class ObjectLink final:public Component
{
public:
    ObjectId Target=0;
    std::string Type()const override{return "ObjectLink";}
    Properties Save()const override{return {{"target",std::to_string(Target)}};}
    void Load(const Properties& p)override
    {
        const auto value=Value(p,"target","0");const auto result=std::from_chars(value.data(),value.data()+value.size(),Target);
        if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size())throw std::runtime_error("Invalid ObjectLink target");
    }
    GameObject* Resolve()const{return Owner().World().Find(Target);}
    std::vector<ObjectId> References()const override{return {Target};}
    void RemapReferences(const std::unordered_map<ObjectId,ObjectId>& remap)override
    {if(const auto it=remap.find(Target);it!=remap.end())Target=it->second;}
};
inline void RegisterCoreComponents(ComponentRegistry& registry)
{registry.Register("ObjectLink",[]{return std::make_unique<ObjectLink>();});}
}
