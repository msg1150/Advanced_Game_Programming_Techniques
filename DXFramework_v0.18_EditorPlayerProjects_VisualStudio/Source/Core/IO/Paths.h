#pragma once
#include "Core/Text/Utf8.h"
#include <filesystem>
#include <stdexcept>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace Paths
{
inline bool ValidUtf8(const std::string& text){return TextUtf8::FromWide(TextUtf8::ToWide(text))==text;}
inline std::filesystem::path FromUtf8(const std::string& text)
{if(!ValidUtf8(text)||text.find('\0')!=std::string::npos)throw std::runtime_error("Invalid UTF-8 path");return std::filesystem::path(std::u8string(text.begin(),text.end()));}
inline std::string ToUtf8(const std::filesystem::path& path)
{const auto text=path.generic_u8string();return {text.begin(),text.end()};}
inline std::filesystem::path Absolute(const std::filesystem::path& path)
{return std::filesystem::absolute(path).lexically_normal();}
inline bool IsWithin(const std::filesystem::path& parent,const std::filesystem::path& path)
{
    const auto base=Absolute(parent),target=Absolute(path);auto a=base.begin(),b=target.begin();
    for(;a!=base.end();++a,++b)
    {
        if(b==target.end())return false;
#ifdef _WIN32
        if(CompareStringOrdinal(a->c_str(),-1,b->c_str(),-1,TRUE)!=CSTR_EQUAL)return false;
#else
        if(*a!=*b)return false;
#endif
    }
    return true;
}
inline std::filesystem::path RelativeMember(const std::filesystem::path& root,const std::string& member)
{
    const auto relative=FromUtf8(member);
    if(member.empty()||relative.is_absolute()||relative.has_root_name())throw std::runtime_error("Project paths must be relative");
    const auto resolved=Absolute(root/relative);
    if(!IsWithin(root,resolved))throw std::runtime_error("Project path escapes its root");return resolved;
}
}
