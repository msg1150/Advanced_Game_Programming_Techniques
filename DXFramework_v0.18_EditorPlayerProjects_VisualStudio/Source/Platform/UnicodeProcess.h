#pragma once
#include <Windows.h>
#include <filesystem>
#include <string>
#include <vector>

namespace Native
{
std::filesystem::path ExecutableDirectory();
std::vector<std::wstring> Arguments();
std::wstring QuoteArgument(const std::wstring& value);
class ChildProcess
{
public:
    ~ChildProcess();
    ChildProcess()=default;ChildProcess(const ChildProcess&)=delete;ChildProcess& operator=(const ChildProcess&)=delete;
    bool Start(const std::filesystem::path& executable,const std::vector<std::wstring>& arguments,
               const std::filesystem::path& workingDirectory,std::wstring& error,bool hidden=false);
    bool Running()const;
    void RequestClose();
private:
    HANDLE process_=nullptr;DWORD pid_=0;
};
}
