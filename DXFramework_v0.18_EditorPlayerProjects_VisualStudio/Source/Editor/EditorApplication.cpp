#include "Editor/EditorApplication.h"
#include "Core/Text/Utf8.h"
#include <algorithm>

bool EditorApplication::Initialize(HINSTANCE instance,const std::filesystem::path& initial)
{
    base_=Native::ExecutableDirectory();
    window_.SetMessageHandler([this](HWND hwnd,UINT msg,WPARAM w,LPARAM l)
    {
        input_.ProcessMessage(hwnd,msg,w,l);
        if(msg==WM_SIZE&&renderer_.IsInitialized()&&LOWORD(l)&&HIWORD(l))
        {uiRenderer_.PrepareForResize();renderer_.Resize(LOWORD(l),HIWORD(l));uiRenderer_.RecreateTarget();camera_.SetAspectRatio(float(LOWORD(l))/HIWORD(l));ui_.Layout(float(LOWORD(l)),float(HIWORD(l)));}
    });
    if(!window_.Initialize(instance,1280,720,L"DX Editor v0.18")||!renderer_.Initialize(window_.GetHandle(),1280,720))
    {error_=L"Editor Window/Renderer 초기화 실패";return false;}
    if(!uiRenderer_.Initialize(renderer_.GetSwapChain())){error_=uiRenderer_.GetError();return false;}
    Engine::ProjectDefinition project;std::string error;
    if(!Engine::ProjectFile::Load(initial.empty()?base_/L"Starter.dxproject":initial,project,error)||!OpenProject(project))
    {error_=error.empty()?status_:TextUtf8::ToWide(error);return false;}
    if(!projects_.Initialize(instance,window_.GetHandle(),base_/L"Starter.dxproject",[this](const auto& p){return OpenProject(p);}))
    {error_=L"Project Launcher 초기화 실패";return false;}
    projects_.SetCurrent(project_);
    ui_.CreateActionButton(L"프로젝트 생성 / 열기",[this]{projects_.Show();});
    ui_.CreateActionButton(L"프로젝트 설정 저장",[this]{SaveProject();});
    ui_.CreateActionButton(L"Player 실행 (저장본)",[this]{LaunchPlayer();});
    ui_.CreateActionButton(L"Player 종료 요청",[this]{player_.RequestClose();});
    runtime_.Attach(ui_,window_.GetHandle());
    auto panel=ui_.CreatePanel(L"Project 상태",L"Project 상태",L"상태 닫기",560,230);
    ui_.AddToPanel(panel,std::make_unique<UILabel>([this]{return TextUtf8::ToWide(project_.Name);}));
    ui_.AddToPanel(panel,std::make_unique<UILabel>([this]{return project_.File.wstring();}));
    ui_.AddToPanel(panel,std::make_unique<UILabel>([this]{return status_;}));
    ui_.Layout(1280,720);timer_.Reset();return true;
}
bool EditorApplication::OpenProject(const Engine::ProjectDefinition& project)
{
    if(!project_.File.empty()&&MessageBoxW(window_.GetHandle(),L"현재 편집 상태를 저장했는지 확인하세요. 프로젝트를 전환할까요?",L"프로젝트 전환",MB_YESNO|MB_ICONQUESTION)!=IDYES)return false;
    EngineRuntimeSettings settings;settings.InitialScene=project.StartupScene;settings.TerrainDirectory=project.TerrainDirectory;
    if(!runtime_.Initialize(renderer_.GetDevice(),renderer_.GetContext(),project.AssetRoot,settings))
    {status_=TextUtf8::ToWide(runtime_.Message());return false;}
    project_=project;camera_.SetPerspective(DirectX::XMConvertToRadians(60),float(renderer_.GetWidth())/std::max(1u,renderer_.GetHeight()),.1f,1200);
    camera_.SetTarget({0,2,0});camera_.SetOrbit(DirectX::XM_PI,DirectX::XMConvertToRadians(47),46);
    controller_.SetMoveSpeed(30);controller_.SetZoomSpeed(3);status_=L"프로젝트를 열었습니다. Player는 디스크의 시작 Scene을 실행합니다.";
    SetWindowTextW(window_.GetHandle(),(L"DX Editor v0.18 — "+TextUtf8::ToWide(project.Name)).c_str());return true;
}
void EditorApplication::SaveProject()
{
    auto copy=project_;copy.StartupScene=runtime_.ScenePath();
    std::string error;if(Engine::ProjectFile::Save(copy,error)){project_=copy;status_=L"현재 Scene을 시작 Scene으로 프로젝트에 저장했습니다.";}else status_=TextUtf8::ToWide(error);
}
void EditorApplication::LaunchPlayer()
{
    player_.Start(base_/L"DXPlayer.exe",{L"--project",project_.File.native()},project_.File.parent_path(),status_);
    if(status_.empty())status_=L"Player 실행 요청 완료 (저장된 시작 Scene)";
}
int EditorApplication::Run()
{
    for(;;)
    {
        input_.BeginFrame();if(!window_.ProcessMessages())break;timer_.Tick();const auto dt=timer_.GetDeltaTime();
        const bool orbit=ui_.Update(input_,float(window_.GetClientWidth()),float(window_.GetClientHeight()));
        runtime_.Update(input_,dt,!ui_.IsKeyboardCaptured(),camera_.GetTarget());
        controller_.Update(camera_,input_,dt,orbit,!ui_.IsKeyboardCaptured()&&!runtime_.CapturesMovementKeys(),!ui_.IsWheelCaptured());
        constexpr float clear[]={.07f,.085f,.11f,1};renderer_.BeginFrame(clear);
        runtime_.Render(renderer_.GetContext(),camera_.GetViewMatrix(),camera_.GetProjectionMatrix(),camera_.GetPosition());
        if(uiRenderer_.Begin(renderer_.GetContext())){ui_.Render(uiRenderer_);uiRenderer_.End();}renderer_.EndFrame();
    }
    player_.RequestClose();return 0;
}
