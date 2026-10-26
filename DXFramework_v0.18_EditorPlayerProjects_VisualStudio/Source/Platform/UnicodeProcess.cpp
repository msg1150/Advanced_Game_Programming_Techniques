#include "Platform/UnicodeProcess.h"
#include <shellapi.h>
#include <stdexcept>

namespace Native
{
std::filesystem::path ExecutableDirectory()
{
    std::vector<wchar_t> path(512);
    for(;;)
    {
        const DWORD count=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
        if(!count)throw std::runtime_error("GetModuleFileNameW failed");
        if(count<path.size())return std::filesystem::path(std::wstring(path.data(),count)).parent_path();
        if(path.size()>=32768)throw std::runtime_error("Executable path exceeds Windows limit");path.resize(path.size()*2);
    }
}
std::vector<std::wstring> Arguments()
{
    int count=0;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!argv)throw std::runtime_error("Cannot parse Unicode command line");
    std::vector<std::wstring> result;try{for(int i=1;i<count;++i)result.emplace_back(argv[i]);}catch(...){LocalFree(argv);throw;}LocalFree(argv);return result;
}
std::wstring QuoteArgument(const std::wstring& value)
{
    // Windows CRT quoting rules: quotes double preceding backslashes; trailing
    // backslashes are doubled before the closing quote. Never invoke a shell.
    std::wstring out=L"\"";std::size_t slashes=0;
    for(wchar_t ch:value)
    {
        if(ch==L'\\'){++slashes;continue;}
        if(ch==L'"'){out.append(slashes*2+1,L'\\');out+=ch;}
        else {out.append(slashes,L'\\');out+=ch;}slashes=0;
    }
    out.append(slashes*2,L'\\');out+=L'"';return out;
}
ChildProcess::~ChildProcess(){if(process_)CloseHandle(process_);}
bool ChildProcess::Start(const std::filesystem::path& executable,const std::vector<std::wstring>& arguments,const std::filesystem::path& workingDirectory,std::wstring& error,bool hidden)
{
    error.clear();if(Running()){error=L"Player가 이미 실행 중입니다.";return false;}
    if(process_){CloseHandle(process_);process_=nullptr;}
    const auto app=std::filesystem::absolute(executable);
    std::wstring command=QuoteArgument(app.native());for(const auto& argument:arguments)command+=L" "+QuoteArgument(argument);
    std::vector<wchar_t> mutableCommand(command.begin(),command.end());mutableCommand.push_back(0);
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION info{};
    if(hidden){startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;}
    if(!CreateProcessW(app.c_str(),mutableCommand.data(),nullptr,nullptr,FALSE,CREATE_UNICODE_ENVIRONMENT|(hidden?CREATE_NO_WINDOW:0),nullptr,
        workingDirectory.empty()?nullptr:workingDirectory.c_str(),&startup,&info))
    {error=L"Player 실행 실패 (Win32 "+std::to_wstring(GetLastError())+L"): "+app.wstring();return false;}
    process_=info.hProcess;pid_=info.dwProcessId;CloseHandle(info.hThread);return true;
}
bool ChildProcess::Running()const
{DWORD code=0;return process_&&GetExitCodeProcess(process_,&code)&&code==STILL_ACTIVE;}
void ChildProcess::RequestClose()
{
    if(!Running())return;
    EnumWindows([](HWND hwnd,LPARAM parameter)->BOOL
    {DWORD process=0;GetWindowThreadProcessId(hwnd,&process);if(process==static_cast<DWORD>(parameter))PostMessageW(hwnd,WM_CLOSE,0,0);return TRUE;},static_cast<LPARAM>(pid_));
}
}
