#include "Editor/ProjectWindow.h"
#include "Platform/FolderDialog.h"
#include "Platform/SceneFileDialogs.h"
#include "Core/Text/Utf8.h"
#include <vector>
namespace
{
std::wstring Text(HWND edit)
{const int n=GetWindowTextLengthW(edit);std::vector<wchar_t> value(static_cast<std::size_t>(n)+1);GetWindowTextW(edit,value.data(),n+1);return value.data();}
}
ProjectWindow::~ProjectWindow(){if(hwnd_)DestroyWindow(hwnd_);if(instance_)UnregisterClassW(L"DXProjectWindow18",instance_);}
bool ProjectWindow::Initialize(HINSTANCE instance,HWND owner,std::filesystem::path file,std::function<bool(const Engine::ProjectDefinition&)> open)
{
    instance_=instance;owner_=owner;template_=std::move(file);open_=std::move(open);
    recents_=std::make_unique<Engine::RecentProjects>(Native::UserStateDirectory()/L"RecentProjects.utf8");recents_->Load();
    WNDCLASSEXW cls{};cls.cbSize=sizeof(cls);cls.lpfnWndProc=Procedure;cls.hInstance=instance;cls.lpszClassName=L"DXProjectWindow18";cls.hCursor=LoadCursor(nullptr,IDC_ARROW);cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);
    if(!RegisterClassExW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
    hwnd_=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,L"프로젝트 생성 / 열기 — 한글 경로 지원",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,680,480,owner,nullptr,instance,this);
    return hwnd_!=nullptr;
}
void ProjectWindow::Show(){RefreshRecent();ShowWindow(hwnd_,SW_SHOW);SetForegroundWindow(hwnd_);}
void ProjectWindow::SetCurrent(const Engine::ProjectDefinition& p){recents_->Add(p.File);RefreshRecent();Status(L"현재 프로젝트: "+TextUtf8::ToWide(p.Name)+L"\r\n"+p.File.wstring());}
void ProjectWindow::Status(const std::wstring& text){SetWindowTextW(status_,text.c_str());}
void ProjectWindow::RefreshRecent()
{if(!recent_)return;SendMessageW(recent_,LB_RESETCONTENT,0,0);for(const auto& file:recents_->Entries())SendMessageW(recent_,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(file.c_str()));}
void ProjectWindow::OpenFile(const std::filesystem::path& path)
{
    Engine::ProjectDefinition project;std::string error;
    if(!Engine::ProjectFile::Load(path,project,error)){Status(TextUtf8::ToWide(error));return;}
    if(open_(project)){SetCurrent(project);ShowWindow(hwnd_,SW_HIDE);}else Status(L"프로젝트 실행 준비 실패. 에디터의 상태 메시지를 확인하세요.");
}
void ProjectWindow::CreateProject()
{
    const auto parent=std::filesystem::path(Text(folder_));const auto name=Text(name_);
    if(parent.empty()||name.empty()){Status(L"상위 폴더와 프로젝트 이름을 입력하세요.");return;}
    const auto relative=std::filesystem::path(name);
    if(relative.has_parent_path()||relative.has_root_name()){Status(L"이름에는 폴더 구분자를 넣을 수 없습니다.");return;}
    Engine::ProjectDefinition project;std::string error;
    if(!Engine::ProjectManager::Create(parent/relative,TextUtf8::FromWide(name),template_,project,error)){Status(TextUtf8::ToWide(error));return;}
    OpenFile(project.File);
}
void ProjectWindow::ChooseProject()
{std::wstring error;const auto file=SceneFileDialogs::Choose(hwnd_,false,error,{L"DX Project (*.dxproject)\0*.dxproject\0\0",L"dxproject",L"프로젝트 열기"});if(file)OpenFile(*file);else if(!error.empty())Status(error);}
void ProjectWindow::OpenRecent()
{const auto selection=SendMessageW(recent_,LB_GETCURSEL,0,0);if(selection!=LB_ERR&&static_cast<std::size_t>(selection)<recents_->Entries().size())OpenFile(recents_->Entries()[selection]);}
LRESULT CALLBACK ProjectWindow::Procedure(HWND hwnd,UINT message,WPARAM w,LPARAM l)
{
    ProjectWindow* self=reinterpret_cast<ProjectWindow*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(message==WM_NCCREATE){self=static_cast<ProjectWindow*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->hwnd_=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(self)
    {try{return self->Message(message,w,l);}catch(const std::exception& e){self->Status(TextUtf8::ToWide(e.what()));return 0;}}
    return DefWindowProcW(hwnd,message,w,l);
}
LRESULT ProjectWindow::Message(UINT message,WPARAM w,LPARAM l)
{
    if(message==WM_CREATE)
    {
        auto control=[this](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id)->HWND
        {auto h=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,hwnd_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance_,nullptr);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return h;};
        control(L"STATIC",L"프로젝트 이름",0,16,16,120,24,0);name_=control(L"EDIT",L"새 게임 프로젝트",WS_BORDER|ES_AUTOHSCROLL,145,14,490,26,10);
        control(L"STATIC",L"상위 폴더",0,16,54,120,24,0);folder_=control(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,145,52,390,26,11);
        control(L"BUTTON",L"폴더 선택",BS_PUSHBUTTON,545,50,90,30,1);
        control(L"BUTTON",L"새 프로젝트 생성",BS_PUSHBUTTON,145,90,180,32,2);control(L"BUTTON",L"프로젝트 파일 열기",BS_PUSHBUTTON,340,90,180,32,3);
        control(L"STATIC",L"최근 프로젝트 (더블클릭으로 열기)",0,16,138,550,24,0);recent_=control(L"LISTBOX",L"",WS_BORDER|LBS_NOTIFY|WS_VSCROLL|WS_HSCROLL,16,164,620,170,4);
        status_=control(L"STATIC",L"프로젝트 이름과 폴더에 한글·공백을 사용할 수 있습니다.",0,16,350,620,70,0);RefreshRecent();return 0;
    }
    if(message==WM_COMMAND)
    {
        switch(LOWORD(w))
        {
        case 1:if(auto path=Native::ChooseFolder(hwnd_,L"프로젝트를 만들 상위 폴더"))SetWindowTextW(folder_,path->c_str());break;
        case 2:CreateProject();break;case 3:ChooseProject();break;case 4:if(HIWORD(w)==LBN_DBLCLK)OpenRecent();break;
        }return 0;
    }
    if(message==WM_CLOSE){ShowWindow(hwnd_,SW_HIDE);return 0;}
    if(message==WM_NCDESTROY){hwnd_=nullptr;return 0;}
    return DefWindowProcW(hwnd_,message,w,l);
}
