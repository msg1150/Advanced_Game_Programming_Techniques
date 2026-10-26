#include "Platform/UnicodeProcess.h"
#include "Core/IO/Paths.h"
#include <fstream>
#include <iomanip>
int wmain()
{
    const auto args=Native::Arguments();if(args.empty())return 2;
    std::ofstream out(std::filesystem::path(args[0]),std::ios::binary);
    out<<std::quoted(Paths::ToUtf8(std::filesystem::current_path()))<<'\n';
    for(std::size_t i=1;i<args.size();++i)out<<std::quoted(TextUtf8::FromWide(args[i]))<<'\n';return out?0:3;
}
