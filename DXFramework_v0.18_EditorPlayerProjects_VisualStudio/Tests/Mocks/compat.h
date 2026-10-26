// Linux clang++-only compatibility shim for MSVC swprintf_s array overload.
#pragma once
#include <cwchar>
#include <cstddef>
template<std::size_t N,typename... Args>
int swprintf_s(wchar_t (&out)[N],const wchar_t* format,Args... args)
{return std::swprintf(out,N,format,args...);}
