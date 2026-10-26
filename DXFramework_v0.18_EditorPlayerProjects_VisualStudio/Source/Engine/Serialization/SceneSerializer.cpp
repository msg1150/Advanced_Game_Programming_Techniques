#include "Engine/Serialization/SceneSerializer.h"
#include "Core/Text/Utf8.h"
#include <iomanip>
#include <istream>
#include <ostream>
#include <set>

namespace Engine
{
namespace
{
bool ValidString(const std::string& text)
{return text.size()<=65536&&TextUtf8::FromWide(TextUtf8::ToWide(text))==text;}
bool Quoted(std::istream& in,std::string& text)
{
    in>>std::ws;if(in.get()!='"')return false;text.clear();
    for(char ch;in.get(ch);)
    {if(ch=='"')return TextUtf8::FromWide(TextUtf8::ToWide(text))==text;
     if(ch=='\\'&&!in.get(ch))return false;if(text.size()>=65536)return false;text+=ch;}
    return false;
}
bool Flag(std::istream& in,bool& value){int flag=-1;if(!(in>>flag)||(flag!=0&&flag!=1))return false;value=flag!=0;return true;}
void ValidateReferences(const Scene& scene)
{
    for(auto* object:scene.Objects())
    {
        unsigned depth=0;auto id=object->Parent();
        while(id){if(++depth>128)throw std::runtime_error("Hierarchy depth exceeds 128");auto* p=scene.Find(id);if(!p)throw std::runtime_error("Missing parent");id=p->Parent();}
        for(auto* c:object->Components())for(auto ref:c->References())if(ref&&!scene.Find(ref))throw std::runtime_error("Missing component object reference");
    }
}
}
bool SceneSerializer::Write(std::ostream& out,const Scene& scene,std::string& error)
{
    error.clear();try
    {
        ValidateReferences(scene);const auto objects=scene.Objects();
        if(!ValidString(scene.Name()))throw std::runtime_error("Invalid scene name");
        if(objects.size()>10000)throw std::runtime_error("Scene object limit exceeded");
        out.imbue(std::locale::classic());out<<std::setprecision(std::numeric_limits<float>::max_digits10);
        out<<"DXENGINE_SCENE 1 "<<std::quoted(scene.Name())<<' '<<objects.size()<<'\n';
        for(auto* object:objects)
        {
            if(!object->Local.Valid()||!ValidString(object->Name))throw std::runtime_error("Invalid transform/object name");
            const auto& t=object->Local;const auto components=object->Components();
            out<<"OBJECT "<<object->Id()<<' '<<object->Parent()<<' '<<int(object->Enabled())<<' '<<std::quoted(object->Name)<<' '
               <<t.Position.x<<' '<<t.Position.y<<' '<<t.Position.z<<' '<<t.Rotation.x<<' '<<t.Rotation.y<<' '<<t.Rotation.z<<' '
               <<t.Scale.x<<' '<<t.Scale.y<<' '<<t.Scale.z<<' '<<components.size()<<'\n';
            if(components.size()>128)throw std::runtime_error("Component limit exceeded");
            for(auto* c:components)
            {
                const auto p=c->Save();if(p.size()>256||c->Type().empty()||!ValidString(c->Type()))throw std::runtime_error("Invalid component/property limit");
                out<<"COMPONENT "<<std::quoted(c->Type())<<' '<<int(c->Enabled())<<' '<<p.size()<<'\n';
                for(const auto& [key,value]:p){if(key.empty()||!ValidString(key)||!ValidString(value))throw std::runtime_error("Invalid property string");out<<std::quoted(key)<<' '<<std::quoted(value)<<'\n';}
            }
        }
        if(!out)throw std::runtime_error("Scene write failed");return true;
    }
    catch(const std::exception& e){error=e.what();return false;}
}
bool SceneSerializer::Read(std::istream& in,const ComponentRegistry& registry,std::unique_ptr<Scene>& output,std::string& error,std::vector<std::string>* warnings)
{
    error.clear();try
    {
        in.imbue(std::locale::classic());std::string tag,name;unsigned version=0;std::size_t count=0;
        if(!(in>>tag>>version)||tag!="DXENGINE_SCENE"||version!=1||!Quoted(in,name)||!(in>>count)||count>10000)throw std::runtime_error("Invalid engine scene header");
        auto candidate=std::make_unique<Scene>(name);std::vector<std::pair<ObjectId,ObjectId>> parents;std::vector<std::string> missing;
        for(std::size_t i=0;i<count;++i)
        {
            ObjectId id=0,parent=0;bool enabled=false;std::size_t componentCount=0;Transform t;
            if(!(in>>tag>>id>>parent)||tag!="OBJECT"||!id||!Flag(in,enabled)||!Quoted(in,name)||
               !(in>>t.Position.x>>t.Position.y>>t.Position.z>>t.Rotation.x>>t.Rotation.y>>t.Rotation.z>>t.Scale.x>>t.Scale.y>>t.Scale.z>>componentCount)||
               !t.Valid()||componentCount>128)throw std::runtime_error("Invalid object record");
            auto& object=candidate->Create(name,id);object.Local=t;object.SetEnabled(enabled);parents.emplace_back(id,parent);
            for(std::size_t j=0;j<componentCount;++j)
            {
                std::string type;std::size_t propertyCount=0;
                if(!(in>>tag)||tag!="COMPONENT"||!Quoted(in,type)||type.empty()||!Flag(in,enabled)||!(in>>propertyCount)||propertyCount>256)throw std::runtime_error("Invalid component record");
                Properties properties;
                for(std::size_t k=0;k<propertyCount;++k)
                {std::string key,value;if(!Quoted(in,key)||!Quoted(in,value)||key.empty()||!properties.emplace(key,value).second)throw std::runtime_error("Invalid/duplicate property");}
                bool absent=false;auto component=registry.Create(type,&absent);component->Load(properties);component->SetEnabled(enabled);
                if(absent)missing.push_back("Missing module: "+type);
                object.Add(std::move(component));
            }
        }
        for(const auto& [id,parent]:parents)if(!candidate->Find(id)->SetParent(parent))throw std::runtime_error("Missing/cyclic parent");
        ValidateReferences(*candidate);in>>std::ws;if(!in.eof())throw std::runtime_error("Trailing scene data");
        output=std::move(candidate);if(warnings)*warnings=std::move(missing);return true;
    }
    catch(const std::exception& e){error=e.what();return false;}
}
}
