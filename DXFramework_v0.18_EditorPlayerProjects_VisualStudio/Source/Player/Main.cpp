#include "Engine/Runtime/RuntimeSession.h"
#include "Engine/Project/Project.h"
#include "Engine/Components/ObjectLink.h"
#if DX_ENABLE_ENGINE_GAMEPLAY
#include "Engine/Gameplay/Movement.h"
#endif
#include "Platform/UnicodeProcess.h"
#include "Platform/Window.h"
#include "Graphics/Renderer.h"
#include "Graphics/Camera.h"
#include "Graphics/CameraController.h"
#include "Input/Input.h"
#include "Core/Timer.h"
#include "Core/IO/AtomicFile.h"
#include <fstream>

namespace
{
bool CheckScene(const Engine::ProjectDefinition& project,std::string& error)
{
    Engine::ComponentRegistry registry;Engine::RegisterCoreComponents(registry);
#if DX_ENABLE_ENGINE_GRAPHICS
    Engine::RegisterGraphicsComponents(registry);
#endif
#if DX_ENABLE_ENGINE_COLLISION
    Engine::RegisterCollisionComponents(registry);
#endif
#if DX_ENABLE_ENGINE_GAMEPLAY
    Engine::RegisterGameplayComponents(registry);
#endif
    std::ifstream in(project.StartupScene,std::ios::binary);std::unique_ptr<Engine::Scene> scene;
    return Engine::SceneSerializer::Read(in,registry,scene,error);
}
int Run(HINSTANCE instance,const Engine::ProjectDefinition& project)
{
    Window window;Input input;Renderer renderer;Camera camera;CameraController controller;Timer timer;
    window.SetMessageHandler([&](HWND hwnd,UINT msg,WPARAM w,LPARAM l)
    {input.ProcessMessage(hwnd,msg,w,l);if(msg==WM_SIZE&&renderer.IsInitialized()&&LOWORD(l)&&HIWORD(l)){renderer.Resize(LOWORD(l),HIWORD(l));camera.SetAspectRatio(float(LOWORD(l))/HIWORD(l));}});
    if(!window.Initialize(instance,1280,720,(L"DX Player — "+TextUtf8::ToWide(project.Name)).c_str())||!renderer.Initialize(window.GetHandle(),1280,720))throw std::runtime_error("Player Window/Renderer failed");
    camera.SetPerspective(DirectX::XMConvertToRadians(60),1280.f/720,.1f,1200);camera.SetTarget({0,2,0});camera.SetOrbit(DirectX::XM_PI,DirectX::XMConvertToRadians(47),46);
    controller.SetMoveSpeed(30);controller.SetZoomSpeed(3);
    Engine::RuntimeSession runtime;Engine::RuntimeSettings settings;settings.InitialScene=project.StartupScene;settings.TerrainDirectory=project.TerrainDirectory;
    if(!runtime.Initialize(renderer.GetDevice(),renderer.GetContext(),project.AssetRoot,settings))throw std::runtime_error(runtime.Status());timer.Reset();
    while(true)
    {
        input.BeginFrame();if(!window.ProcessMessages())break;timer.Tick();const float dt=timer.GetDeltaTime();
        Engine::Vec3 direction{float(input.IsKeyDown('D'))-float(input.IsKeyDown('A')),0,float(input.IsKeyDown('W'))-float(input.IsKeyDown('S'))};
        controller.Update(camera,input,dt,true,!DX_ENABLE_ENGINE_GAMEPLAY,true);runtime.Update(dt,direction,true,camera.GetTarget());
        constexpr float clear[]={.07f,.085f,.11f,1};renderer.BeginFrame(clear);runtime.Render(renderer.GetContext(),camera.GetViewMatrix(),camera.GetProjectionMatrix(),camera.GetPosition());renderer.EndFrame();
    }
    return 0;
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int)
{
    bool check=false;std::filesystem::path file,report;std::string error;int result=1;
    try
    {
        const auto args=Native::Arguments();
        for(std::size_t i=0;i<args.size();++i)
        {
            if(args[i]==L"--check-project")check=true;
            else if((args[i]==L"--project"||args[i]==L"--report")&&i+1<args.size())
            {const bool project=args[i]==L"--project";if(project)file=args[++i];else report=args[++i];}
            else throw std::runtime_error("Usage: DXPlayer --project <path> [--check-project] [--report <path>]");
        }
        if(file.empty())file=Native::ExecutableDirectory()/L"Starter.dxproject";
        Engine::ProjectDefinition project;if(!Engine::ProjectFile::Load(file,project,error))throw std::runtime_error(error);
        if(check){if(!CheckScene(project,error))throw std::runtime_error(error);result=0;}
        else
        {
            const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(com))throw std::runtime_error("COM failed");
            try{result=Run(instance,project);}catch(...){CoUninitialize();throw;}CoUninitialize();
        }
    }
    catch(const std::exception& e){error=e.what();if(!check)MessageBoxW(nullptr,TextUtf8::ToWide(error).c_str(),L"DX Player",MB_OK|MB_ICONERROR);}
    if(!report.empty()){std::wstring wideError;if(!Files::WriteReplacement(report,result==0?"PASS\n":error,wideError))return 2;}
    return result;
}
