#include "Core/IO/AtomicFile.h"
#include <atomic>
#include <algorithm>
#ifdef _WIN32
#include <Windows.h>
#else
#include <fstream>
#include <chrono>
#endif
namespace Files
{
bool WriteReplacement(const std::filesystem::path& path,const std::string& bytes,std::wstring& error)
{
    error.clear();static std::atomic<unsigned long long> counter{0};auto temp=path;
#ifdef _WIN32
    HANDLE file=INVALID_HANDLE_VALUE;
    for(unsigned attempt=0;attempt<100;++attempt)
    {
        temp=path;temp+=L".writing."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(++counter);
        file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE)break;if(GetLastError()!=ERROR_FILE_EXISTS)break;
    }
    if(file==INVALID_HANDLE_VALUE){error=L"임시 파일을 만들 수 없습니다: "+path.wstring();return false;}
    bool okay=true;std::size_t offset=0;
    while(offset<bytes.size())
    {
        const DWORD count=static_cast<DWORD>(std::min<std::size_t>(bytes.size()-offset,1024u*1024u));DWORD written=0;
        if(!WriteFile(file,bytes.data()+offset,count,&written,nullptr)||written!=count){okay=false;break;}offset+=written;
    }
    okay=FlushFileBuffers(file)&&okay;okay=CloseHandle(file)&&okay;
    if(okay&&MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;
    DeleteFileW(temp.c_str());error=L"파일 저장 실패. 기존 파일을 유지합니다: "+path.wstring();return false;
#else
    temp+=".writing."+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"."+std::to_string(++counter);
    {std::ofstream out(temp,std::ios::binary);out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));out.close();if(!out){std::filesystem::remove(temp);error=L"Write failed";return false;}}
    std::error_code ec;std::filesystem::rename(temp,path,ec);if(ec){std::filesystem::remove(temp);error=L"Replace failed";return false;}return true;
#endif
}
}
