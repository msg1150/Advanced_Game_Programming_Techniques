#pragma once
#include "Engine/Core/Transform.h"
#include <charconv>
#include <map>
#include <sstream>
#include <iomanip>
#include <locale>
#include <limits>

namespace Engine
{
using Properties=std::map<std::string,std::string>;
inline std::string Value(const Properties& p,const std::string& key,std::string fallback={})
{const auto it=p.find(key);return it==p.end()?fallback:it->second;}
inline float Number(const Properties& p,const std::string& key,float fallback)
{
    const auto it=p.find(key);if(it==p.end())return fallback;
    float value=0;const auto& s=it->second;
    const auto r=std::from_chars(s.data(),s.data()+s.size(),value);
    if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size()||!std::isfinite(value))throw std::runtime_error("Invalid number: "+key);
    return value;
}
inline std::string Text(float v)
{std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(std::numeric_limits<float>::max_digits10)<<v;return out.str();}
inline Vec3 Vector(const Properties& p,const std::string& key,Vec3 fallback)
{
    return {Number(p,key+".x",fallback.x),Number(p,key+".y",fallback.y),Number(p,key+".z",fallback.z)};
}
inline void Put(Properties& p,const std::string& key,Vec3 v)
{p[key+".x"]=Text(v.x);p[key+".y"]=Text(v.y);p[key+".z"]=Text(v.z);}
}
