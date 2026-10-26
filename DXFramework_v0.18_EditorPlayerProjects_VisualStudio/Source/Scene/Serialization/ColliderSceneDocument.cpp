#include "Scene/Serialization/ColliderSceneDocument.h"
#include "Core/Text/Utf8.h"
#include <cmath>
#include <iomanip>
#include <istream>
#include <limits>
#include <locale>
#include <ostream>

namespace
{
    constexpr std::size_t MaxColliders=100000,MaxNameBytes=4096;
    bool Finite(const Collision::Vec3& v)
    {return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
    bool Valid(const Collision::ColliderComponent& c)
    {
        if(!Finite(c.Position)||!Finite(c.Rotation)||c.Shape.valueless_by_exception()||
           TextUtf8::FromWide(c.Name).size()>MaxNameBytes)return false;
        return std::visit([](const auto& s)
        {
            using T=std::decay_t<decltype(s)>;
            if constexpr(std::is_same_v<T,Collision::SphereShape>)
                return std::isfinite(s.Radius)&&s.Radius>=0.01f;
            else if constexpr(std::is_same_v<T,Collision::CapsuleShape>)
                return std::isfinite(s.Radius)&&s.Radius>=0.01f&&std::isfinite(s.HalfSegment)&&s.HalfSegment>=0.f;
            else return Finite(s.HalfExtents)&&s.HalfExtents.x>=0.01f&&s.HalfExtents.y>=0.01f&&s.HalfExtents.z>=0.01f;
        },c.Shape);
    }
    // Bounded quoted string: malformed files cannot allocate an unbounded name.
    bool ReadName(std::istream& in,std::string& name)
    {
        in>>std::ws;
        if(in.get()!='"')return false;
        for(char ch;in.get(ch);)
        {
            if(ch=='"')return true;
            if(ch=='\\'&&!in.get(ch))return false;
            if(name.size()>=MaxNameBytes)return false;
            name.push_back(ch);
        }
        return false;
    }
}

bool ColliderSceneSerializer::Read(std::istream& in,ColliderSceneDocument& output,std::wstring& error)
{
    error.clear();in.imbue(std::locale::classic());
    std::string magic;unsigned version=0;std::size_t count=0;
    if(!(in>>magic>>version>>count)||magic!="DXCOLLIDER_SCENE"||version!=ColliderSceneDocument::Version||count>MaxColliders)
    {error=L"Scene header/version/count is invalid.";return false;}
    ColliderSceneDocument candidate;
    candidate.Colliders.reserve(count);
    for(std::size_t i=0;i<count;++i)
    {
        Collision::ColliderComponent c;std::string shape,name;int enabled=0,trigger=0;
        if(!(in>>shape)||!ReadName(in,name)||!(in>>c.Position.x>>c.Position.y>>c.Position.z
             >>c.Rotation.x>>c.Rotation.y>>c.Rotation.z>>c.Layer>>c.Mask>>enabled>>trigger)||
             (enabled!=0&&enabled!=1)||(trigger!=0&&trigger!=1))
        {error=L"Scene collider record is invalid.";return false;}
        c.Name=TextUtf8::ToWide(name);
        if(TextUtf8::FromWide(c.Name)!=name){error=L"Scene name must be valid UTF-8.";return false;}
        c.Enabled=enabled!=0;c.IsTrigger=trigger!=0;
        if(shape=="Sphere") {Collision::SphereShape s;if(!(in>>s.Radius))return error=L"Invalid sphere.",false;c.Shape=s;}
        else if(shape=="Capsule") {Collision::CapsuleShape s;if(!(in>>s.Radius>>s.HalfSegment))return error=L"Invalid capsule.",false;c.Shape=s;}
        else if(shape=="AABB"||shape=="OBB")
        {
            Collision::Vec3 e;if(!(in>>e.x>>e.y>>e.z))return error=L"Invalid box.",false;
            if(shape=="AABB")c.Shape=Collision::AABBShape{e};else c.Shape=Collision::OBBShape{e};
        }
        else {error=L"Unknown collider shape.";return false;}
        if(!Valid(c)){error=L"Scene contains non-finite values or invalid shape dimensions.";return false;}
        candidate.Colliders.push_back(std::move(c));
    }
    in>>std::ws;
    if(!in.eof()){error=L"Unexpected trailing scene data.";return false;}
    output=std::move(candidate);return true;
}

bool ColliderSceneSerializer::Write(std::ostream& out,const ColliderSceneDocument& doc,std::wstring& error)
{
    error.clear();
    if(doc.Colliders.size()>MaxColliders){error=L"Too many colliders.";return false;}
    for(const auto& c:doc.Colliders)if(!Valid(c)){error=L"Cannot save invalid collider data.";return false;}
    out.imbue(std::locale::classic());out<<std::setprecision(std::numeric_limits<float>::max_digits10);
    out<<"DXCOLLIDER_SCENE "<<ColliderSceneDocument::Version<<' '<<doc.Colliders.size()<<'\n';
    for(const auto& c:doc.Colliders)
    {
        const char* shape[]={"Sphere","AABB","OBB","Capsule"};
        out<<shape[c.Shape.index()]<<' '<<std::quoted(TextUtf8::FromWide(c.Name))<<' '
           <<c.Position.x<<' '<<c.Position.y<<' '<<c.Position.z<<' '
           <<c.Rotation.x<<' '<<c.Rotation.y<<' '<<c.Rotation.z<<' '
           <<c.Layer<<' '<<c.Mask<<' '<<int(c.Enabled)<<' '<<int(c.IsTrigger)<<' ';
        std::visit([&out](const auto& s)
        {
            using T=std::decay_t<decltype(s)>;
            if constexpr(std::is_same_v<T,Collision::SphereShape>)out<<s.Radius;
            else if constexpr(std::is_same_v<T,Collision::CapsuleShape>)out<<s.Radius<<' '<<s.HalfSegment;
            else out<<s.HalfExtents.x<<' '<<s.HalfExtents.y<<' '<<s.HalfExtents.z;
        },c.Shape);
        out<<'\n';
    }
    if(!out){error=L"Scene write failed.";return false;}return true;
}
