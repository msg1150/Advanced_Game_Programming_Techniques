#include "Platform/UnicodeProcess.h"
#include "Core/IO/Paths.h"
#include <cassert>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <thread>
int wmain(int argc,wchar_t** argv)
{
    assert(argc>=3);const auto base=Paths::Absolute(std::filesystem::path(argv[2]))/L"한글 실행 폴더 & (공백)";
    std::filesystem::create_directories(base);const auto helper=base/L"인자 확인 도구.exe";
    std::filesystem::copy_file(std::filesystem::path(argv[1]),helper,std::filesystem::copy_options::overwrite_existing);
    const auto report=base/L"전달 결과.utf8";std::wstring error;Native::ChildProcess process;
    const std::vector<std::wstring> values{L"한글 日本語 😀 공백",L"",L"인용 \"문자\" 와 끝 역슬래시\\",L"A&B (괄호)"};
    auto arguments=values;arguments.insert(arguments.begin(),report.native());assert(process.Start(helper,arguments,base,error,true));
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(process.Running()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(5));
    assert(!process.Running());std::ifstream in(report,std::ios::binary);std::string received;assert(in>>std::quoted(received));assert(Paths::FromUtf8(received)==base);
    for(const auto& expected:values){assert(in>>std::quoted(received));assert(received==TextUtf8::FromWide(expected));}
}
