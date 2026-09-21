// ============================================================================
// Application.cpp — Legacy Showcase는 초기화 / Render 대상에서 완전히 제외.
// ============================================================================
#include "Core/Application.h"
#include <algorithm>
#include <filesystem>
#include <sstream>
using namespace DirectX;
namespace
{
    std::filesystem::path ExecutableDirectory()
    {
        wchar_t filename[MAX_PATH]={};
        const DWORD count=GetModuleFileNameW(nullptr,filename,MAX_PATH);
        return count>0 && count<MAX_PATH
            ? std::filesystem::path(filename).parent_path()
            : std::filesystem::current_path();
    }
}
bool Application::Fail(const wchar_t* stage,const std::wstring& message)
{
    lastError_=std::wstring(stage)+L"\n\n"+message;
    OutputDebugStringW(lastError_.c_str());return false;
}
bool Application::Initialize(HINSTANCE instance)
{
    lastError_.clear();
    window_.SetMessageHandler([this](HWND hwnd,UINT msg,WPARAM w,LPARAM l)
    {HandleWindowMessage(hwnd,msg,w,l);});
    if(!window_.Initialize(instance,1280,720,L"DirectX 11 - Large Disk Terrain Streaming"))
        return Fail(L"Window",L"Window 생성 실패");
    if(!renderer_.Initialize(window_.GetHandle(),window_.GetClientWidth(),
                             window_.GetClientHeight()))
        return Fail(L"Renderer",L"D3D11 Renderer 초기화 실패");
    if(!debugText_.Initialize(renderer_.GetSwapChain()))
        return Fail(L"Debug Overlay",debugText_.GetLastErrorMessage());
    const float aspect=float(window_.GetClientWidth())/
                       float(std::max(1u,window_.GetClientHeight()));
    camera_.SetPerspective(XMConvertToRadians(60.f),aspect,.1f,1200.f);
    camera_.SetTarget({0.f,2.f,0.f});
    camera_.SetOrbit(XMConvertToRadians(180.f),XMConvertToRadians(47.f),46.f);
    cameraController_.SetMoveSpeed(30.f);
    cameraController_.SetZoomSpeed(3.f);
    const auto dir=ExecutableDirectory();
    TriplanarMaterialDesc material={};
    const auto textures=dir/L"Assets/Textures/Terrain";
    material.GrassTexturePath=textures/L"Grass.png";
    material.RockTexturePath=textures/L"Rock.png";
    material.SnowTexturePath=textures/L"Snow.png";
    material.ProjectionScale=.38f;
    material.BlendSharpness=4.f;
    material.GrassFadeStartHeight=.6f;
    material.GrassFadeEndHeight=3.5f;
    material.RockSlopeStart=.12f;
    material.RockSlopeEnd=.42f;
    material.SnowStartHeight=7.f;
    material.SnowFullHeight=12.f;
    DiskTerrainSettings settings={};
    // 기준 Zoom Distance=46에서는 예전과 같은 Load 56 / Unload 84.
    settings.LoadRadius=56.f;
    settings.UnloadRadius=84.f;

    // 마우스 휠 Zoom Out -> 로딩 반경 증가, Zoom In -> 감소.
    // LoadRadius는 28~160, UnloadRadius는 최대 188로 하드캡.
    // 이미 로드된 Tile은 UnloadMargin(28)까지 유지한다.
    settings.ZoomRadius.Enabled=true;
    settings.ZoomRadius.ReferenceCameraDistance=46.0f;
    settings.ZoomRadius.MinimumLoadRadius=28.0f;
    settings.ZoomRadius.MaximumLoadRadius=160.0f;
    settings.ZoomRadius.RadiusPerDistance=0.75f;
    settings.ZoomRadius.UnloadMargin=28.0f;
    settings.ZoomRadius.MaximumUnloadRadius=188.0f;
    settings.MaxPending=12u;
    settings.MaxGpuUploadsPerFrame=2u;
    settings.LOD.LeafCellSize=8u;
    settings.LOD.SplitDistanceFactor=2.5f;
    settings.LOD.SkirtDepth=.25f;
    if(!terrain_.Initialize(renderer_.GetDevice(),renderer_.GetContext(),
                            dir/L"Assets/TerrainTiles",dir/L"Shaders/TriplanarTerrain",
                            material,settings))
        return Fail(L"Disk Terrain",terrain_.GetError());
    // 첫 Frame 이전에 중심 Tile을 요청해 Loading이 즉시 시작되게 한다.
    terrain_.Update(camera_.GetTarget(),camera_.GetDistance());
    timer_.Reset();initialized_=true;return true;
}
int Application::Run()
{
    while(true)
    {
        input_.BeginFrame();
        if(!window_.ProcessMessages())break;
        timer_.Tick();
        Update(timer_.GetDeltaTime());
        Render();
    }
    return 0;
}
void Application::HandleWindowMessage(HWND hwnd,UINT msg,WPARAM w,LPARAM l)
{
    input_.ProcessMessage(hwnd,msg,w,l);
    if(msg==WM_SIZE && renderer_.IsInitialized())
    {
        const UINT width=LOWORD(l),height=HIWORD(l);
        if(width && height)
        {
            if(debugText_.IsInitialized())debugText_.PrepareForResize();
            renderer_.Resize(width,height);
            if(debugText_.IsInitialized())debugText_.RecreateTarget();
            camera_.SetAspectRatio(float(width)/float(height));
        }
    }
}
void Application::Update(float dt)
{
    if(input_.IsKeyPressed(VK_F1))terrain_.SetCulling(!terrain_.IsCulling());
    if(input_.IsKeyPressed(VK_F2))terrain_.SetLOD(!terrain_.IsLOD());
    if(input_.IsKeyPressed(VK_F3))debugPanel_=!debugPanel_;
    if(input_.IsKeyPressed(VK_F4))terrain_.SetBorders(!terrain_.IsBorders());
    if(input_.IsKeyPressed(VK_F5))terrain_.SetStreaming(!terrain_.IsStreaming());
    if(input_.IsKeyPressed(VK_F6))miniMap_=!miniMap_;
    cameraController_.Update(camera_,input_,dt);
    terrain_.Update(camera_.GetTarget(),camera_.GetDistance());
}
void Application::Render()
{
    constexpr float clearColor[4]={.07f,.085f,.11f,1.f};
    renderer_.BeginFrame(clearColor);
    terrain_.Render(renderer_.GetContext(),camera_.GetViewMatrix(),
                    camera_.GetProjectionMatrix(),camera_.GetPosition(),miniMap_);
    if(debugPanel_)
    {
        const auto& stats=terrain_.GetStats();
        std::wstringstream info;
        info << L"[F3] Hide Info   [F6] MiniMap: " << (miniMap_?L"ON":L"OFF")
             << L"\n[F1] Culling: " << (terrain_.IsCulling()?L"ON":L"OFF")
             << L"  [F2] LOD: " << (terrain_.IsLOD()?L"ON":L"OFF")
             << L"  [F4] Tile Border: " << (terrain_.IsBorders()?L"ON":L"OFF")
             << L"\n[F5] Disk Streaming: " << (terrain_.IsStreaming()?L"ON":L"OFF")
             << L"  Zoom: " << camera_.GetDistance()
             << L"  Load: " << terrain_.GetActiveLoadRadius()
             << L"  Unload: " << terrain_.GetActiveUnloadRadius()
             << L"  Max Load: " << terrain_.GetMaximumLoadRadius()
             << L"\nTiles Loaded / Total: " << stats.LoadedTiles << L" / " << stats.TotalTiles
             << L"  Desired: " << stats.DesiredTiles << L"  Pending: " << stats.PendingTiles
             << L"\nVisible: " << stats.VisibleTiles << L"  Culled: " << stats.CulledTiles
             << L"  Failed: " << stats.FailedTiles
             << L"\nTriangles: " << stats.SurfaceTriangles << L" / "
             << stats.FullWorldTriangles << L"  Draw Calls: " << stats.DrawCalls
             << L"\nDisk Reads: " << stats.DiskReads
             << L"  Disk Read: " << stats.DiskBytes/1024u << L" KiB"
             << L"\nResident GPU Mesh Buffers: " << stats.ResidentGpuMeshBytes/1024u << L" KiB"
             << L"  CPU full HeightMap: 0 KiB";
        if(!terrain_.GetError().empty())info << L"\nERROR: " << terrain_.GetError();
        debugText_.DrawTextBlock(renderer_.GetContext(),info.str(),false);
    }
    else
    {
        debugText_.DrawTextBlock(renderer_.GetContext(),L"[F3] Info  [F6] Map",true);
    }
    renderer_.EndFrame();
}
const std::wstring& Application::GetLastErrorMessage()const{return lastError_;}
