// ============================================================================
// UIUtf8.h
// ----------------------------------------------------------------------------
// UI 문서 파일은 UTF-8, 현재 DirectWrite Widget 문자열은 std::wstring을 사용한다.
// Win32 API에 의존하지 않는 작은 변환기라 Portable Test에서도 동일하게 사용한다.
// ============================================================================
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
namespace UIUtf8
{
    inline void AppendWide(std::wstring& out,std::uint32_t cp)
    {
        if constexpr(sizeof(wchar_t)>=4)
        {
            out.push_back(static_cast<wchar_t>(cp));
        }
        else
        {
            if(cp<=0xFFFFu)out.push_back(static_cast<wchar_t>(cp));
            else
            {
                cp-=0x10000u;
                out.push_back(static_cast<wchar_t>(0xD800u+(cp>>10u)));
                out.push_back(static_cast<wchar_t>(0xDC00u+(cp&0x3FFu)));
            }
        }
    }
    inline std::wstring ToWide(std::string_view input)
    {
        std::wstring out;out.reserve(input.size());
        for(std::size_t i=0;i<input.size();)
        {
            const auto lead=static_cast<unsigned char>(input[i]);
            std::uint32_t cp=0;std::size_t count=0;
            if(lead<0x80u){cp=lead;count=1;}
            else if((lead&0xE0u)==0xC0u){cp=lead&0x1Fu;count=2;}
            else if((lead&0xF0u)==0xE0u){cp=lead&0x0Fu;count=3;}
            else if((lead&0xF8u)==0xF0u){cp=lead&0x07u;count=4;}
            else{AppendWide(out,0xFFFDu);++i;continue;}
            if(i+count>input.size()){AppendWide(out,0xFFFDu);break;}
            bool valid=true;
            for(std::size_t j=1;j<count;++j)
            {
                const auto next=static_cast<unsigned char>(input[i+j]);
                if((next&0xC0u)!=0x80u){valid=false;break;}
                cp=(cp<<6u)|(next&0x3Fu);
            }
            if(!valid || cp>0x10FFFFu || (cp>=0xD800u&&cp<=0xDFFFu) ||
               (count==2&&cp<0x80u) || (count==3&&cp<0x800u) || (count==4&&cp<0x10000u))
            {AppendWide(out,0xFFFDu);++i;continue;}
            AppendWide(out,cp);i+=count;
        }
        return out;
    }
    inline void AppendUtf8(std::string& out,std::uint32_t cp)
    {
        if(cp<=0x7Fu)out.push_back(static_cast<char>(cp));
        else if(cp<=0x7FFu)
        {out.push_back(static_cast<char>(0xC0u|(cp>>6u)));out.push_back(static_cast<char>(0x80u|(cp&0x3Fu)));}
        else if(cp<=0xFFFFu)
        {out.push_back(static_cast<char>(0xE0u|(cp>>12u)));out.push_back(static_cast<char>(0x80u|((cp>>6u)&0x3Fu)));out.push_back(static_cast<char>(0x80u|(cp&0x3Fu)));}
        else
        {out.push_back(static_cast<char>(0xF0u|(cp>>18u)));out.push_back(static_cast<char>(0x80u|((cp>>12u)&0x3Fu)));out.push_back(static_cast<char>(0x80u|((cp>>6u)&0x3Fu)));out.push_back(static_cast<char>(0x80u|(cp&0x3Fu)));}
    }
    inline std::string FromWide(std::wstring_view input)
    {
        std::string out;out.reserve(input.size()*2u);
        for(std::size_t i=0;i<input.size();++i)
        {
            std::uint32_t cp=static_cast<std::uint32_t>(input[i]);
            if constexpr(sizeof(wchar_t)==2)
            {
                if(cp>=0xD800u&&cp<=0xDBFFu&&i+1<input.size())
                {
                    const auto low=static_cast<std::uint32_t>(input[i+1]);
                    if(low>=0xDC00u&&low<=0xDFFFu)
                    {cp=0x10000u+((cp-0xD800u)<<10u)+(low-0xDC00u);++i;}
                }
            }
            AppendUtf8(out,cp);
        }
        return out;
    }
}
