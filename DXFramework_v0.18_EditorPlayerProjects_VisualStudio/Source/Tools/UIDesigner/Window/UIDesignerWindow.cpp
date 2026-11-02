// ============================================================================
// UIDesignerWindow.cpp
// ----------------------------------------------------------------------------
// v0.2 Designer Shell: 별도 Win32 창 + Palette / Hierarchy / Inspector.
// 저장/불러오기는 Runtime과 동일한 .ui.xml(UIDocumentDefinition)을 사용합니다.
// 중앙 Canvas Preview는 UIDesignerCanvas 모듈이 Custom UI Framework로 렌더링합니다.
// ============================================================================
#include "Tools/UIDesigner/Window/UIDesignerWindow.h"
#include "UI/Document/Serialization/UIUtf8.h"
#include <commdlg.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace
{
constexpr wchar_t kDesignerClass[]=L"DXFrameworkUIDesignerWindow";

// Toolbar
constexpr int ID_NEW=1001,ID_OPEN=1002,ID_SAVE=1003,ID_SAVE_AS=1004;
// Palette / Hierarchy
constexpr int ID_PALETTE=1101,ID_ADD=1102;
constexpr int ID_HIERARCHY=1201,ID_DELETE=1202,ID_UP=1203,ID_DOWN=1204;
// Inspector
constexpr int ID_ID=1301,ID_TEXT=1302,ID_BINDING=1303,ID_MIN=1304,ID_MAX=1305,ID_STEP=1306;
constexpr int ID_WIDTH=1307,ID_HEIGHT=1308,ID_OPEN_LABEL=1309,ID_CLOSE_LABEL=1310,ID_APPLY=1311;
constexpr int ID_STATUS=1401,ID_CANVAS=1501;

const wchar_t* TypeDisplay(UIElementType type)
{
    switch(type)
    {
    case UIElementType::Panel:return L"Panel";
    case UIElementType::Column:return L"Column";
    case UIElementType::Label:return L"Label";
    case UIElementType::CheckBox:return L"CheckBox";
    case UIElementType::Slider:return L"Slider";
    case UIElementType::Button:return L"Button";
    default:return L"-";
    }
}


std::wstring DiagnosticsText(const std::vector<UIDiagnostic>& diagnostics)
{
    std::wstringstream ss;
    const std::size_t count=std::min<std::size_t>(diagnostics.size(),8u);
    for(std::size_t i=0;i<count;++i)
    {
        const auto& d=diagnostics[i];
        ss<<(d.Severity==UIDiagnosticSeverity::Error?L"오류":L"경고")
          <<L" ["<<d.Line<<L":"<<d.Column<<L"] "<<UIUtf8::ToWide(d.Message)<<L"\r\n";
    }
    if(diagnostics.size()>count)ss<<L"... 외 "<<(diagnostics.size()-count)<<L"개";
    return ss.str();
}
}

UIDesignerWindow::~UIDesignerWindow()
{
    if(hwnd_)DestroyWindow(hwnd_);
    if(font_)DeleteObject(font_);
    if(instance_)
    {
        UnregisterClassW(kDesignerClass,instance_);
    }
}

bool UIDesignerWindow::Initialize(HINSTANCE instance,std::filesystem::path assetDirectory,
                                  std::filesystem::path initialDocument)
{
    instance_=instance;
    assetDirectory_=std::move(assetDirectory);
    initialDocument_=std::move(initialDocument);
    lastError_.clear();
    if(!RegisterWindowClasses())
    {
        lastError_=L"UI Designer Win32 Window Class 등록에 실패했습니다.";
        return false;
    }
    return true;
}

bool UIDesignerWindow::RegisterWindowClasses()
{
    WNDCLASSEXW existing{};existing.cbSize=sizeof(existing);
    if(!GetClassInfoExW(instance_,kDesignerClass,&existing))
    {
        WNDCLASSEXW wc{};
        wc.cbSize=sizeof(wc);wc.style=CS_HREDRAW|CS_VREDRAW;
        wc.lpfnWndProc=WindowProc;wc.hInstance=instance_;
        wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
        wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
        wc.lpszClassName=kDesignerClass;
        if(!RegisterClassExW(&wc))return false;
    }
    return true;
}

bool UIDesignerWindow::CreateDesignerWindow()
{
    hwnd_=CreateWindowExW(WS_EX_APPWINDOW,kDesignerClass,L"DXFramework UI Designer",
        WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1420,850,
        nullptr,nullptr,instance_,this);
    if(!hwnd_)
    {
        lastError_=L"UI Designer 창 생성에 실패했습니다.";
        return false;
    }
    return true;
}

void UIDesignerWindow::Show()
{
    if(hwnd_)
    {
        if(IsIconic(hwnd_))ShowWindow(hwnd_,SW_RESTORE);
        ShowWindow(hwnd_,SW_SHOW);
        SetForegroundWindow(hwnd_);
        return;
    }
    if(!CreateDesignerWindow())
    {
        MessageBoxW(nullptr,lastError_.c_str(),L"UI Designer",MB_OK|MB_ICONERROR);
        return;
    }

    if(!initialLoaded_)
    {
        initialLoaded_=true;
        if(!initialDocument_.empty() && std::filesystem::exists(initialDocument_))
        {
            std::vector<UIDiagnostic> diagnostics;
            if(!model_.Load(initialDocument_,diagnostics))
            {
                const auto text=DiagnosticsText(diagnostics);
                MessageBoxW(hwnd_,text.c_str(),L"초기 UI 문서 Load 실패",MB_OK|MB_ICONWARNING);
                model_.NewDocument();
            }
        }
    }
    selection_=model_.FirstPanel();
    dirty_=false;
    RefreshAll();
    ShowWindow(hwnd_,SW_SHOW);
    UpdateWindow(hwnd_);
}

LRESULT CALLBACK UIDesignerWindow::WindowProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam)
{
    UIDesignerWindow* self=nullptr;
    if(message==WM_NCCREATE)
    {
        auto* create=reinterpret_cast<CREATESTRUCTW*>(lParam);
        self=static_cast<UIDesignerWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        if(self)self->hwnd_=hwnd;
    }
    else self=reinterpret_cast<UIDesignerWindow*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    return self?self->HandleMessage(message,wParam,lParam):DefWindowProcW(hwnd,message,wParam,lParam);
}

LRESULT UIDesignerWindow::HandleMessage(UINT message,WPARAM wParam,LPARAM lParam)
{
    switch(message)
    {
    case WM_CREATE:
        CreateControls();
        return 0;
    case WM_SIZE:
        LayoutControls(LOWORD(lParam),HIWORD(lParam));
        return 0;
    case WM_GETMINMAXINFO:
        {
            auto* info=reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x=1040;info->ptMinTrackSize.y=650;
        }
        return 0;
    case WM_COMMAND:
        {
            const int id=LOWORD(wParam),notify=HIWORD(wParam);
            switch(id)
            {
            case ID_NEW:if(notify==BN_CLICKED && ConfirmDiscardChanges())NewDocument();break;
            case ID_OPEN:if(notify==BN_CLICKED && ConfirmDiscardChanges())OpenDocument();break;
            case ID_SAVE:if(notify==BN_CLICKED)SaveDocument();break;
            case ID_SAVE_AS:if(notify==BN_CLICKED)SaveDocumentAs();break;
            case ID_ADD:if(notify==BN_CLICKED)AddPaletteSelection();break;
            case ID_PALETTE:if(notify==LBN_DBLCLK)AddPaletteSelection();break;
            case ID_HIERARCHY:if(notify==LBN_SELCHANGE)OnHierarchySelectionChanged();break;
            case ID_DELETE:if(notify==BN_CLICKED)DeleteSelection();break;
            case ID_UP:if(notify==BN_CLICKED)MoveSelection(-1);break;
            case ID_DOWN:if(notify==BN_CLICKED)MoveSelection(1);break;
            case ID_APPLY:if(notify==BN_CLICKED)ApplyInspector();break;
            default:break;
            }
        }
        return 0;
    case WM_CLOSE:
        if(ConfirmDiscardChanges())DestroyWindow(hwnd_);
        return 0;
    case WM_DESTROY:
        if(font_){DeleteObject(font_);font_=nullptr;}
        canvas_.Destroy();paletteList_=nullptr;hierarchyList_=nullptr;status_=nullptr;
        typeValue_=idEdit_=textEdit_=bindingEdit_=minEdit_=maxEdit_=stepEdit_=nullptr;
        widthEdit_=heightEdit_=openLabelEdit_=closeLabelEdit_=nullptr;
        return 0; // 절대 PostQuitMessage를 호출하지 않는다. 메인 게임 창이 계속 실행된다.
    case WM_NCDESTROY:
        {
            // WM_DESTROY 뒤에도 WM_NCDESTROY가 오므로 HWND를 너무 일찍 null로 만들지 않는다.
            // Default Proc에 유효 HWND를 전달한 뒤 객체 연결을 해제한다.
            HWND dying=hwnd_;
            SetWindowLongPtrW(dying,GWLP_USERDATA,0);
            hwnd_=nullptr;
            return DefWindowProcW(dying,message,wParam,lParam);
        }
    }
    return DefWindowProcW(hwnd_,message,wParam,lParam);
}

void UIDesignerWindow::ApplyFont(HWND control)const
{
    if(control && font_)SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);
}

HWND UIDesignerWindow::CreateLabel(const wchar_t* text,int id)
{
    HWND h=CreateWindowExW(0,L"STATIC",text,WS_CHILD|WS_VISIBLE|SS_LEFT,
        0,0,100,22,hwnd_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance_,nullptr);
    ApplyFont(h);return h;
}

HWND UIDesignerWindow::CreateEdit(int id)
{
    HWND h=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,
        0,0,100,24,hwnd_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance_,nullptr);
    ApplyFont(h);return h;
}

void UIDesignerWindow::CreateControls()
{
    font_=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,HANGUL_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Malgun Gothic");
    const auto button=[this](const wchar_t* text,int id)
    {
        HWND h=CreateWindowExW(0,L"BUTTON",text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
            0,0,90,30,hwnd_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance_,nullptr);
        ApplyFont(h);return h;
    };
    button(L"새 문서",ID_NEW);button(L"열기",ID_OPEN);button(L"저장",ID_SAVE);button(L"다른 이름으로",ID_SAVE_AS);

    CreateLabel(L"Widget Palette");
    paletteList_=CreateWindowExW(WS_EX_CLIENTEDGE,L"LISTBOX",L"",
        WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_TABSTOP|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,
        0,0,200,140,hwnd_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PALETTE)),instance_,nullptr);ApplyFont(paletteList_);
    const struct {const wchar_t* Text;UIElementType Type;} palette[]={
        {L"Panel",UIElementType::Panel},{L"Column",UIElementType::Column},{L"Label",UIElementType::Label},
        {L"Button",UIElementType::Button},{L"CheckBox",UIElementType::CheckBox},{L"Slider",UIElementType::Slider}};
    for(const auto& item:palette)
    {
        const LRESULT index=SendMessageW(paletteList_,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(item.Text));
        SendMessageW(paletteList_,LB_SETITEMDATA,static_cast<WPARAM>(index),static_cast<LPARAM>(item.Type));
    }
    SendMessageW(paletteList_,LB_SETCURSEL,2,0);
    button(L"선택 Widget 추가",ID_ADD);

    CreateLabel(L"Hierarchy");
    hierarchyList_=CreateWindowExW(WS_EX_CLIENTEDGE,L"LISTBOX",L"",
        WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_TABSTOP|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,
        0,0,200,300,hwnd_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_HIERARCHY)),instance_,nullptr);ApplyFont(hierarchyList_);
    button(L"삭제",ID_DELETE);button(L"위로",ID_UP);button(L"아래로",ID_DOWN);

    CreateLabel(L"Inspector");
    CreateLabel(L"Type");typeValue_=CreateLabel(L"-");
    CreateLabel(L"ID");idEdit_=CreateEdit(ID_ID);
    CreateLabel(L"Text / Title");textEdit_=CreateEdit(ID_TEXT);
    CreateLabel(L"Binding / Action");bindingEdit_=CreateEdit(ID_BINDING);
    CreateLabel(L"Min");minEdit_=CreateEdit(ID_MIN);
    CreateLabel(L"Max");maxEdit_=CreateEdit(ID_MAX);
    CreateLabel(L"Step");stepEdit_=CreateEdit(ID_STEP);
    CreateLabel(L"Width");widthEdit_=CreateEdit(ID_WIDTH);
    CreateLabel(L"Height");heightEdit_=CreateEdit(ID_HEIGHT);
    CreateLabel(L"Open Label");openLabelEdit_=CreateEdit(ID_OPEN_LABEL);
    CreateLabel(L"Close Label");closeLabelEdit_=CreateEdit(ID_CLOSE_LABEL);
    button(L"속성 적용",ID_APPLY);

    status_=CreateWindowExW(0,L"STATIC",L"준비",WS_CHILD|WS_VISIBLE|SS_LEFT,
        0,0,300,22,hwnd_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_STATUS)),instance_,nullptr);ApplyFont(status_);
    if(!canvas_.Create(instance_,hwnd_,ID_CANVAS,[this](const UIDesignerPath& path)
    {
        SelectPath(path);
    }))
        SetStatus(L"Canvas 초기화 실패: "+canvas_.GetLastError());

    // WM_SIZE에서 찾기 쉽도록 기본 Win32 child의 생성 순서를 유지한다.
    RefreshAll();
}

void UIDesignerWindow::LayoutControls(int width,int height)
{
    if(width<=0||height<=0)return;
    const int margin=10,toolbarH=34,statusH=24,leftW=230,rightW=310,gap=10;
    const int bodyY=margin+toolbarH+gap;
    const int bodyH=std::max(100,height-bodyY-statusH-margin*2);
    const int canvasX=margin+leftW+gap;
    const int canvasW=std::max(100,width-leftW-rightW-gap*2-margin*2);
    const int inspectorX=canvasX+canvasW+gap;

    // Toolbar button handles are identified by ID.
    MoveWindow(GetDlgItem(hwnd_,ID_NEW),margin,margin,82,toolbarH,TRUE);
    MoveWindow(GetDlgItem(hwnd_,ID_OPEN),margin+88,margin,82,toolbarH,TRUE);
    MoveWindow(GetDlgItem(hwnd_,ID_SAVE),margin+176,margin,82,toolbarH,TRUE);
    MoveWindow(GetDlgItem(hwnd_,ID_SAVE_AS),margin+264,margin,116,toolbarH,TRUE);

    // Left labels are unnamed IDs, so enumerate visible STATIC controls by known creation order is fragile.
    // 대신 Palette/Hierarchy 실제 control 위치를 기준으로 제목은 SetWindowPos를 하지 않고 기본 위치를
    // 아래 EnumChildWindows lambda에서 텍스트로 찾아 배치합니다.
    const auto placeStatic=[this](const wchar_t* text,int x,int y,int w,int h)
    {
        struct Search{const wchar_t* text;HWND found=nullptr;} search{text};
        EnumChildWindows(hwnd_,[](HWND child,LPARAM data)->BOOL
        {
            auto* s=reinterpret_cast<Search*>(data);wchar_t buffer[128]={};
            GetWindowTextW(child,buffer,128);
            wchar_t cls[32]={};GetClassNameW(child,cls,32);
            if(wcscmp(cls,L"Static")==0 && wcscmp(buffer,s->text)==0){s->found=child;return FALSE;}
            return TRUE;
        },reinterpret_cast<LPARAM>(&search));
        if(search.found)MoveWindow(search.found,x,y,w,h,TRUE);
    };

    placeStatic(L"Widget Palette",margin,bodyY,leftW,22);
    const int paletteY=bodyY+24,paletteH=145;
    MoveWindow(paletteList_,margin,paletteY,leftW,paletteH,TRUE);
    MoveWindow(GetDlgItem(hwnd_,ID_ADD),margin,paletteY+paletteH+6,leftW,30,TRUE);
    const int hierarchyTitleY=paletteY+paletteH+42;
    placeStatic(L"Hierarchy",margin,hierarchyTitleY,leftW,22);
    const int hierarchyY=hierarchyTitleY+24;
    const int hierarchyH=std::max(90,bodyY+bodyH-hierarchyY-38);
    MoveWindow(hierarchyList_,margin,hierarchyY,leftW,hierarchyH,TRUE);
    const int smallW=(leftW-12)/3;
    MoveWindow(GetDlgItem(hwnd_,ID_DELETE),margin,hierarchyY+hierarchyH+5,smallW,29,TRUE);
    MoveWindow(GetDlgItem(hwnd_,ID_UP),margin+smallW+6,hierarchyY+hierarchyH+5,smallW,29,TRUE);
    MoveWindow(GetDlgItem(hwnd_,ID_DOWN),margin+(smallW+6)*2,hierarchyY+hierarchyH+5,smallW,29,TRUE);

    canvas_.SetBounds(canvasX,bodyY,canvasW,bodyH);

    placeStatic(L"Inspector",inspectorX,bodyY,rightW,22);
    int y=bodyY+28;
    auto field=[&](const wchar_t* label,HWND edit)
    {
        placeStatic(label,inspectorX,y,110,22);
        MoveWindow(edit,inspectorX+112,y-2,rightW-112,25,TRUE);y+=31;
    };
    placeStatic(L"Type",inspectorX,y,110,22);MoveWindow(typeValue_,inspectorX+112,y,rightW-112,22,TRUE);y+=31;
    field(L"ID",idEdit_);field(L"Text / Title",textEdit_);field(L"Binding / Action",bindingEdit_);
    field(L"Min",minEdit_);field(L"Max",maxEdit_);field(L"Step",stepEdit_);
    field(L"Width",widthEdit_);field(L"Height",heightEdit_);
    field(L"Open Label",openLabelEdit_);field(L"Close Label",closeLabelEdit_);
    MoveWindow(GetDlgItem(hwnd_,ID_APPLY),inspectorX,y+5,rightW,32,TRUE);

    MoveWindow(status_,margin,height-statusH-margin,width-margin*2,statusH,TRUE);
}

void UIDesignerWindow::NewDocument()
{
    model_.NewDocument();selection_=model_.FirstPanel();dirty_=false;
    SetStatus(L"새 UIDocument를 만들었습니다.");RefreshAll();
}

bool UIDesignerWindow::ConfirmDiscardChanges()
{
    if(!dirty_)return true;
    const int result=MessageBoxW(hwnd_,L"저장하지 않은 변경 내용이 있습니다. 저장할까요?",
        L"UI Designer",MB_YESNOCANCEL|MB_ICONQUESTION);
    if(result==IDCANCEL)return false;
    if(result==IDYES)return SaveDocument();
    return true;
}

void UIDesignerWindow::OpenDocument()
{
    wchar_t file[4096]={};
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=hwnd_;
    dialog.lpstrFilter=L"DXFramework UI (*.ui.xml)\0*.ui.xml\0XML (*.xml)\0*.xml\0모든 파일 (*.*)\0*.*\0\0";
    dialog.lpstrFile=file;dialog.nMaxFile=4096;
    const std::wstring initial=assetDirectory_.wstring();dialog.lpstrInitialDir=initial.empty()?nullptr:initial.c_str();
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_EXPLORER;
    if(!GetOpenFileNameW(&dialog))return;
    std::vector<UIDiagnostic> diagnostics;
    if(!model_.Load(file,diagnostics))
    {
        const auto text=DiagnosticsText(diagnostics);
        MessageBoxW(hwnd_,text.c_str(),L"UI XML 열기 실패",MB_OK|MB_ICONERROR);return;
    }
    selection_=model_.FirstPanel();dirty_=false;
    if(!diagnostics.empty())MessageBoxW(hwnd_,DiagnosticsText(diagnostics).c_str(),L"UI XML 경고",MB_OK|MB_ICONWARNING);
    SetStatus(L"문서를 열었습니다: "+model_.CurrentPath().wstring());RefreshAll();
}

bool UIDesignerWindow::SaveDocument()
{
    if(model_.CurrentPath().empty())return SaveDocumentAs();
    if(!model_.Save(model_.CurrentPath()))
    {
        MessageBoxW(hwnd_,L"UI XML 파일을 저장하지 못했습니다.",L"UI Designer",MB_OK|MB_ICONERROR);return false;
    }
    dirty_=false;SetStatus(L"저장 완료: "+model_.CurrentPath().wstring());UpdateWindowTitle();return true;
}

bool UIDesignerWindow::SaveDocumentAs()
{
    wchar_t file[4096]={};
    if(!model_.CurrentPath().empty())wcsncpy_s(file,model_.CurrentPath().c_str(),_TRUNCATE);
    else
    {
        const auto suggested=(assetDirectory_/L"NewUI.ui.xml").wstring();
        wcsncpy_s(file,suggested.c_str(),_TRUNCATE);
    }
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=hwnd_;
    dialog.lpstrFilter=L"DXFramework UI (*.ui.xml)\0*.ui.xml\0XML (*.xml)\0*.xml\0\0";
    dialog.lpstrFile=file;dialog.nMaxFile=4096;dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_EXPLORER;
    const std::wstring initial=assetDirectory_.wstring();dialog.lpstrInitialDir=initial.empty()?nullptr:initial.c_str();
    if(!GetSaveFileNameW(&dialog))return false;
    std::filesystem::path path=file;
    if(path.extension()!=L".xml")path+=L".ui.xml";
    if(!model_.Save(path))
    {
        MessageBoxW(hwnd_,L"UI XML 파일을 저장하지 못했습니다.",L"UI Designer",MB_OK|MB_ICONERROR);return false;
    }
    model_.SetCurrentPath(path);dirty_=false;SetStatus(L"저장 완료: "+path.wstring());UpdateWindowTitle();return true;
}

void UIDesignerWindow::AddPaletteSelection()
{
    const LRESULT index=SendMessageW(paletteList_,LB_GETCURSEL,0,0);
    if(index==LB_ERR)return;
    const auto type=static_cast<UIElementType>(SendMessageW(paletteList_,LB_GETITEMDATA,index,0));
    const auto added=model_.AddElement(type,selection_);
    if(!added.Valid){SetStatus(L"현재 선택 위치에는 해당 Element를 추가할 수 없습니다.");return;}
    selection_=added;MarkDirty();SetStatus(L"Element를 추가했습니다.");RefreshAll();
}

void UIDesignerWindow::DeleteSelection()
{
    UIDesignerPath next;
    if(!model_.Remove(selection_,next)){SetStatus(L"삭제할 Element를 선택하세요.");return;}
    selection_=next.Valid?next:model_.FirstPanel();MarkDirty();SetStatus(L"선택 Element를 삭제했습니다.");RefreshAll();
}

void UIDesignerWindow::MoveSelection(int direction)
{
    UIDesignerPath moved;
    if(!model_.Move(selection_,direction,moved)){SetStatus(L"현재 Element는 더 이동할 수 없습니다.");return;}
    selection_=moved;MarkDirty();SetStatus(direction<0?L"위로 이동했습니다.":L"아래로 이동했습니다.");RefreshAll();
}

void UIDesignerWindow::OnHierarchySelectionChanged()
{
    const LRESULT selected=SendMessageW(hierarchyList_,LB_GETCURSEL,0,0);
    if(selected==LB_ERR || static_cast<std::size_t>(selected)>=hierarchyItems_.size())return;
    SelectPath(hierarchyItems_[static_cast<std::size_t>(selected)].Path);
}

void UIDesignerWindow::SelectPath(const UIDesignerPath& path)
{
    selection_=path;RefreshInspector();RefreshHierarchy();canvas_.SetSelection(selection_);
}

std::wstring UIDesignerWindow::EditText(HWND edit)
{
    const int length=GetWindowTextLengthW(edit);std::wstring value(static_cast<std::size_t>(length),L'\0');
    if(length>0)GetWindowTextW(edit,value.data(),length+1);return value;
}

void UIDesignerWindow::SetEditText(HWND edit,const std::wstring& value)
{if(edit)SetWindowTextW(edit,value.c_str());}

std::wstring UIDesignerWindow::Wide(std::string_view utf8){return UIUtf8::ToWide(utf8);}
std::string UIDesignerWindow::Utf8(std::wstring_view wide){return UIUtf8::FromWide(wide);}

bool UIDesignerWindow::TryParsePositive(const std::wstring& text,float minimum,float& value)
{
    if(text.empty())return false;
    wchar_t* end=nullptr;const double parsed=std::wcstod(text.c_str(),&end);
    if(end!=text.c_str()+text.size() || !std::isfinite(parsed) || parsed<minimum)return false;
    value=static_cast<float>(parsed);return true;
}

void UIDesignerWindow::ApplyInspector()
{
    auto* element=model_.Resolve(selection_);if(!element)return;
    if(element->Type==UIElementType::Column){SetStatus(L"Column은 현재 직접 편집할 속성이 없습니다.");return;}

    UIDesignerModel::SetAttribute(*element,"id",Utf8(EditText(idEdit_)),true);
    const std::string text=Utf8(EditText(textEdit_));
    const std::string binding=Utf8(EditText(bindingEdit_));

    if(element->Type==UIElementType::Panel)
    {
        float width=0.f,height=0.f;
        if(!TryParsePositive(EditText(widthEdit_),120.f,width) || !TryParsePositive(EditText(heightEdit_),90.f,height))
        {MessageBoxW(hwnd_,L"Panel Width는 120 이상, Height는 90 이상의 숫자여야 합니다.",L"Inspector",MB_OK|MB_ICONWARNING);return;}
        UIDesignerModel::SetAttribute(*element,"title",text);
        UIDesignerModel::SetAttribute(*element,"width",Utf8(EditText(widthEdit_)));
        UIDesignerModel::SetAttribute(*element,"height",Utf8(EditText(heightEdit_)));
        UIDesignerModel::SetAttribute(*element,"openLabel",Utf8(EditText(openLabelEdit_)));
        UIDesignerModel::SetAttribute(*element,"closeLabel",Utf8(EditText(closeLabelEdit_)));
    }
    else if(element->Type==UIElementType::Label)
    {
        UIDesignerModel::SetAttribute(*element,"text",text);
        UIDesignerModel::SetAttribute(*element,"textBinding",binding,true);
    }
    else if(element->Type==UIElementType::Button)
    {
        UIDesignerModel::SetAttribute(*element,"text",text);
        UIDesignerModel::SetAttribute(*element,"action",binding,true);
    }
    else if(element->Type==UIElementType::CheckBox)
    {
        UIDesignerModel::SetAttribute(*element,"text",text);
        UIDesignerModel::SetAttribute(*element,"binding",binding,true);
    }
    else if(element->Type==UIElementType::Slider)
    {
        float minimum=0.f,maximum=0.f,step=0.f;
        wchar_t* e1=nullptr;wchar_t* e2=nullptr;wchar_t* e3=nullptr;
        const auto minText=EditText(minEdit_),maxText=EditText(maxEdit_),stepText=EditText(stepEdit_);
        minimum=static_cast<float>(std::wcstod(minText.c_str(),&e1));
        maximum=static_cast<float>(std::wcstod(maxText.c_str(),&e2));
        step=static_cast<float>(std::wcstod(stepText.c_str(),&e3));
        if(e1!=minText.c_str()+minText.size()||e2!=maxText.c_str()+maxText.size()||
           e3!=stepText.c_str()+stepText.size()||!std::isfinite(minimum)||!std::isfinite(maximum)||
           !std::isfinite(step)||maximum<=minimum||step<=0.f)
        {MessageBoxW(hwnd_,L"Slider는 Max > Min, Step > 0 조건의 숫자가 필요합니다.",L"Inspector",MB_OK|MB_ICONWARNING);return;}
        UIDesignerModel::SetAttribute(*element,"text",text);
        UIDesignerModel::SetAttribute(*element,"binding",binding,true);
        UIDesignerModel::SetAttribute(*element,"min",Utf8(minText));
        UIDesignerModel::SetAttribute(*element,"max",Utf8(maxText));
        UIDesignerModel::SetAttribute(*element,"step",Utf8(stepText));
    }

    MarkDirty();SetStatus(L"Inspector 속성을 적용했습니다.");RefreshAll();
}

void UIDesignerWindow::RefreshHierarchy()
{
    if(!hierarchyList_)return;
    hierarchyItems_=model_.BuildHierarchy();SendMessageW(hierarchyList_,LB_RESETCONTENT,0,0);
    int selectedIndex=-1;
    for(std::size_t i=0;i<hierarchyItems_.size();++i)
    {
        const auto& item=hierarchyItems_[i];
        std::wstring text(static_cast<std::size_t>(item.Depth)*2u,L' ');text+=Wide(item.DisplayUtf8);
        SendMessageW(hierarchyList_,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str()));
        if(item.Path==selection_)selectedIndex=static_cast<int>(i);
    }
    if(selectedIndex>=0)SendMessageW(hierarchyList_,LB_SETCURSEL,selectedIndex,0);
}

void UIDesignerWindow::RefreshInspector()
{
    const auto* element=model_.Resolve(selection_);
    if(!element)
    {
        SetWindowTextW(typeValue_,L"-");
        for(HWND h:{idEdit_,textEdit_,bindingEdit_,minEdit_,maxEdit_,stepEdit_,widthEdit_,heightEdit_,openLabelEdit_,closeLabelEdit_})
        {SetEditText(h,L"");EnableWindow(h,FALSE);}
        EnableWindow(GetDlgItem(hwnd_,ID_APPLY),FALSE);return;
    }
    SetWindowTextW(typeValue_,TypeDisplay(element->Type));EnableWindow(GetDlgItem(hwnd_,ID_APPLY),element->Type!=UIElementType::Column);
    const auto set=[&](HWND edit,const char* attr,bool enabled)
    {EnableWindow(edit,enabled);SetEditText(edit,enabled?Wide(UIDesignerModel::Attribute(*element,attr)):L"");};

    const bool panel=element->Type==UIElementType::Panel;
    const bool slider=element->Type==UIElementType::Slider;
    const bool widget=element->Type==UIElementType::Label||element->Type==UIElementType::CheckBox||slider||element->Type==UIElementType::Button;
    set(idEdit_,"id",panel||widget);
    set(textEdit_,panel?"title":"text",panel||widget);
    const char* bindAttr=element->Type==UIElementType::Label?"textBinding":
                         element->Type==UIElementType::Button?"action":"binding";
    set(bindingEdit_,bindAttr,widget);
    set(minEdit_,"min",slider);set(maxEdit_,"max",slider);set(stepEdit_,"step",slider);
    set(widthEdit_,"width",panel);set(heightEdit_,"height",panel);
    set(openLabelEdit_,"openLabel",panel);set(closeLabelEdit_,"closeLabel",panel);
}

void UIDesignerWindow::RefreshAll()
{
    RefreshHierarchy();RefreshInspector();UpdateWindowTitle();
    canvas_.SetModel(&model_);canvas_.SetSelection(selection_);canvas_.Invalidate();
}

void UIDesignerWindow::UpdateWindowTitle()
{
    if(!hwnd_)return;
    std::wstring title=L"DXFramework UI Designer";
    if(!model_.CurrentPath().empty())title+=L" - "+model_.CurrentPath().filename().wstring();
    else title+=L" - 새 문서";
    if(dirty_)title+=L" *";SetWindowTextW(hwnd_,title.c_str());
}

void UIDesignerWindow::SetStatus(std::wstring text){if(status_)SetWindowTextW(status_,text.c_str());}
void UIDesignerWindow::MarkDirty(){dirty_=true;UpdateWindowTitle();}

