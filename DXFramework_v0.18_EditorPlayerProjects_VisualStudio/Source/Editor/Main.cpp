#include "Editor/EditorApplication.h"
#include "Platform/UnicodeProcess.h"
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int)
{
    const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(com))return 1;int result=1;
    try
    {
        std::filesystem::path project;const auto args=Native::Arguments();
        if(!args.empty()){if(args.size()!=2||args[0]!=L"--project")throw std::runtime_error("Usage: DXEditor --project <path>");project=args[1];}
        {EditorApplication app;if(app.Initialize(instance,project))result=app.Run();else MessageBoxW(nullptr,app.Error().c_str(),L"DX Editor",MB_OK|MB_ICONERROR);}
    }
    catch(const std::exception& e){const auto text=TextUtf8::ToWide(e.what());MessageBoxW(nullptr,text.c_str(),L"DX Editor",MB_OK|MB_ICONERROR);}
    CoUninitialize();return result;
}
