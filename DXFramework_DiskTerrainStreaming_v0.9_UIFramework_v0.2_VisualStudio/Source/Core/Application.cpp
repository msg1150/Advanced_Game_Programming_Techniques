// ============================================================================
// Application.cpp : Disk Terrain + 프레임워크 자체 제작 UI 한 화면.
// 이전 F1~F6 토글은 모두 TerrainSettingsPanel CheckBox로 이동했다.
// ============================================================================
#include "Core/Application.h"
#include <algorithm>
#include <filesystem>
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
    OutputDebugStringW(lastError_.c_str());
    return false;
}
bool Application::Initialize(HINSTANCE instance)
{
    lastError_.clear();
    window_.SetMessageHandler([this](HWND hwnd,UINT msg,WPARAM w,LPARAM l)
    {HandleWindowMessage(hwnd,msg,w,l);});
    if(!window_.Initialize(instance,1280,720,L"DirectX 11 - 대형 지형 / 사용자 UI"))
        return Fail(L"Window",L"Window 생성 실패");
    if(!renderer_.Initialize(window_.GetHandle(),window_.GetClientWidth(),
                             window_.GetClientHeight()))
        return Fail(L"Renderer",L"D3D11 Renderer 초기화 실패");

    // 자체 제작 Direct2D/DirectWrite UI Renderer. 기존 Debug Renderer 중복 초기화 금지.
    if(!uiRenderer_.Initialize(renderer_.GetSwapChain()))
        return Fail(L"UI Renderer",uiRenderer_.GetError());

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
    settings.LoadRadius=56.f;
    settings.UnloadRadius=84.f;
    settings.RadiusLimits.MinimumLoad=28.f;
    settings.RadiusLimits.MaximumLoad=160.f;
    settings.RadiusLimits.UnloadMargin=28.f;
    settings.RadiusLimits.MaximumUnload=188.f;
    settings.MaxPending=12u;
    settings.MaxGpuUploadsPerFrame=2u;
    settings.LOD.LeafCellSize=8u;
    settings.LOD.SplitDistanceFactor=2.5f;
    settings.LOD.SkirtDepth=.25f;
    if(!terrain_.Initialize(renderer_.GetDevice(),renderer_.GetContext(),
                            dir/L"Assets/TerrainTiles",dir/L"Shaders/TriplanarTerrain",
                            material,settings))
        return Fail(L"Disk Terrain",terrain_.GetError());

    // Feature는 UIWidget의 Getter/Setter에만 바인딩한다.
    // UIManager / UIRenderer는 Terrain의 헤더를 알 필요가 없다.
    terrainSettingsPanel_.Register(uiManager_,terrain_,debugPanel_,miniMap_);
    uiDemoPanel_.Register(uiManager_); // 동일 UIManager에 별도 Feature 등록 (Terrain 의존성 없음).
    uiManager_.Layout(static_cast<float>(window_.GetClientWidth()),
                      static_cast<float>(window_.GetClientHeight()));

    // 첫 프레임 이전 중심 Tile 요청. 휠 거리값은 Streaming에 전달하지 않는다.
    terrain_.Update(camera_.GetTarget());
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
            // UI Target이 BackBuffer를 붙잡고 있으면 ResizeBuffers가 실패한다.
            uiRenderer_.PrepareForResize();
            renderer_.Resize(width,height);
            if(!uiRenderer_.RecreateTarget())
                OutputDebugStringW(L"[CustomUI] WM_SIZE: D2D Target 재생성 실패\n");
            camera_.SetAspectRatio(float(width)/float(height));
            uiManager_.Layout(static_cast<float>(width),static_cast<float>(height));
        }
    }
}
void Application::Update(float dt)
{
    // 기능 ON/OFF를 담당하던 F1~F6 처리는 삭제.
    // UI 입력이 MouseDown에서 시작하면 Release까지 카메라 Orbit 차단.
    const bool allowOrbit=uiManager_.Update(
        input_,static_cast<float>(window_.GetClientWidth()),
        static_cast<float>(window_.GetClientHeight()));

    // 휠 Zoom과 WASD 이동은 항상 기존 방식으로 작동한다.
    cameraController_.Update(
        camera_,input_,dt,allowOrbit,!uiManager_.IsKeyboardCaptured());
    // Zoom 값과 완전히 분리된 UI Load Radius로만 거리 판정한다.
    terrain_.Update(camera_.GetTarget(),dt);
}
void Application::Render()
{
    constexpr float clearColor[4]={.07f,.085f,.11f,1.f};
    renderer_.BeginFrame(clearColor);

    // 3D Terrain과 D3D 기반 MiniMap 렌더가 먼저 끝나야 BackBuffer 공유 가능.
    terrain_.Render(renderer_.GetContext(),camera_.GetViewMatrix(),
                    camera_.GetProjectionMatrix(),camera_.GetPosition(),miniMap_);
    if(uiRenderer_.Begin(renderer_.GetContext()))
    {
        // 읽기 전용 통계를 먼저, 드래그 가능한 설정창을 맨 마지막에 그린다.
        // 패널이 왼쪽으로 이동해 통계 패널과 겹치더라도 클릭할 설정이 가려지지 않는다.
        terrainSettingsPanel_.RenderStats(
            uiRenderer_,terrain_,camera_.GetDistance(),
            static_cast<float>(window_.GetClientWidth()),debugPanel_);
        uiManager_.Render(uiRenderer_);
        uiRenderer_.End();
    }
    renderer_.EndFrame();
}
const std::wstring& Application::GetLastErrorMessage()const{return lastError_;}
