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
    if(!window_.Initialize(instance,1280,720,L"DirectX 11 v0.16 - Modular Scene / Terrain Tools"))
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

    // QuadTree LOD Geomorphing은 기존 Split 기준을 바꾸지 않는다.
    // Parent가 Child로 바뀌는 경계 안쪽 25% 구간에서만 부드럽게 보간한다.
    settings.GeomorphEnabled=true;
    settings.Geomorph.TransitionRatio=0.25f;
    if(!terrain_.Initialize(renderer_.GetDevice(),renderer_.GetContext(),
                            dir/L"Assets/TerrainTiles",dir/L"Shaders/TriplanarTerrain",
                            material,settings))
        return Fail(L"Disk Terrain",terrain_.GetError());

    // Collision / GPU Picking은 Terrain과 독립 Feature로 초기화한다.
    // Physics Core는 DirectX에 의존하지 않고, 이 Scene Facade가 Debug Render와
    // GPU Picking을 연결한다. 따라서 Feature 제거 시 기존 Terrain 코드는 유지된다.
    if(!collisionScene_.Initialize(renderer_.GetDevice(),dir/L"Shaders"))
        return Fail(L"Collision / GPU Picking",collisionScene_.GetError());

    // Feature는 UIWidget의 Getter/Setter에만 바인딩한다.
    // UIManager / UIRenderer는 Terrain의 헤더를 알 필요가 없다.
    terrainSettingsPanel_.Register(uiManager_,terrain_,debugPanel_,miniMap_);
    // 우리가 만든 Custom UI에 Collision 생성/선택/편집 Panel을 등록한다.
    collisionScene_.RegisterUI(uiManager_);
#if DX_ENABLE_SCENE_PERSISTENCE
    scenePersistence_.Attach(uiManager_,collisionScene_,window_.GetHandle());
#endif
#if DX_ENABLE_TERRAIN_QUERY
    std::wstring queryError;
    if(terrainQuerySource_.Initialize(dir/L"Assets/TerrainTiles",4u*1024u*1024u,queryError))
    {
#if DX_ENABLE_TERRAIN_PLACEMENT
        terrainPlacement_=std::make_unique<TerrainPlacementFeature>(terrainQuery_);
        terrainPlacement_->Attach(uiManager_,collisionScene_);
#endif
    }
    else OutputDebugStringW((L"[TerrainQuery] "+queryError+L"\n").c_str());
#endif
    // UIDocument는 실행 파일 옆 Assets/UI에서 읽습니다. Visual Studio PostBuild가 Assets를 복사합니다.
    // 실패하면 UIDemoPanel 내부 Code-driven Fallback이 동작하므로 Terrain 실행은 유지됩니다.
    uiDemoPanel_.Register(uiManager_,dir/L"Assets/UI/UIDemo.ui.xml");

    // UI Designer는 게임 BackBuffer와 완전히 분리된 별도 Top-Level Win32 창입니다.
    // 실패해도 Terrain Runtime을 막지는 않고, 버튼을 눌렀을 때 오류를 확인할 수 있도록 합니다.
    if(!uiDesigner_.Initialize(instance,dir/L"Assets/UI",dir/L"Assets/UI/UIDemo.ui.xml"))
        OutputDebugStringW((L"[UIDesigner] "+uiDesigner_.GetLastError()+L"\n").c_str());
    uiManager_.CreateActionButton(L"UI Designer",[this]{uiDesigner_.Show();});

    uiManager_.Layout(static_cast<float>(window_.GetClientWidth()),
                      static_cast<float>(window_.GetClientHeight()));

    // 첫 프레임 이전 중심 Tile 요청. 휠 거리값은 Streaming에 전달하지 않는다.
    terrain_.Update(camera_.GetTarget());
    collisionScene_.SetSpawnAnchor(camera_.GetTarget());
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
    // 생성 버튼을 누르는 순간에도 현재 Camera Target 주변에 배치되도록
    // UI Update 전에 Spawn Anchor를 먼저 동기화한다.
    collisionScene_.SetSpawnAnchor(camera_.GetTarget());

    // 기능 ON/OFF를 담당하던 F1~F6 처리는 삭제.
    // UI 입력이 MouseDown에서 시작하면 Release까지 카메라 Orbit 차단.
    const bool allowOrbit=uiManager_.Update(
        input_,static_cast<float>(window_.GetClientWidth()),
        static_cast<float>(window_.GetClientHeight()));

    // UI가 소비하지 않은 Scene의 Left Click만 GPU Picking 요청으로 전달한다.
    // 같은 allowOrbit 값을 사용하므로 UI Slider/Button 클릭이 3D 선택으로 새지 않는다.
    bool toolOwnsPointer=false;
#if DX_ENABLE_TERRAIN_PLACEMENT
    if(terrainPlacement_)toolOwnsPointer=terrainPlacement_->Update(input_,allowOrbit,camera_,
        window_.GetClientWidth(),window_.GetClientHeight(),collisionScene_);
#endif
    collisionScene_.UpdateInput(input_,allowOrbit&&!toolOwnsPointer);

    // Scroll 가능한 UI Panel 위에서는 Wheel을 UI가 소비합니다.
    // 그 외 영역에서는 기존 Camera Zoom이 그대로 작동합니다.
    cameraController_.Update(
        camera_,input_,dt,allowOrbit&&!toolOwnsPointer,!uiManager_.IsKeyboardCaptured(),
        !uiManager_.IsWheelCaptured());
    // Zoom 값과 완전히 분리된 UI Load Radius로만 거리 판정한다.
    terrain_.Update(camera_.GetTarget(),dt);

    // Camera가 이번 Frame에 이동했을 수 있으므로 다음 생성 위치와 Collision 상태를 갱신한다.
    collisionScene_.SetSpawnAnchor(camera_.GetTarget());
    collisionScene_.UpdateCollisions();
}
void Application::Render()
{
    constexpr float clearColor[4]={.07f,.085f,.11f,1.f};
    renderer_.BeginFrame(clearColor);

    const XMMATRIX view=camera_.GetViewMatrix();
    const XMMATRIX projection=camera_.GetProjectionMatrix();

    // 3D Terrain이 먼저 Scene Depth를 기록한다.
    // GPU Picking은 이 Depth를 재사용해서 Terrain 뒤에 가려진 Collider Proxy를 제외한다.
    terrain_.Render(renderer_.GetContext(),view,projection,camera_.GetPosition(),miniMap_);

    collisionScene_.ProcessGPUPicking(
        renderer_.GetContext(),renderer_.GetDepthStencilView(),view,projection,
        renderer_.GetWidth(),renderer_.GetHeight());

    // Picking Pass가 끝난 뒤 실제 화면에는 Wireframe Debug Collider만 그린다.
    collisionScene_.RenderDebug(renderer_.GetContext(),view,projection);

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
