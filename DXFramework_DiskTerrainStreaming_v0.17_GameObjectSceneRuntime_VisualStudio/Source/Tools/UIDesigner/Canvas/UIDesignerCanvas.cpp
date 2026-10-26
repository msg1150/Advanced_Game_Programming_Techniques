// ============================================================================
// UIDesignerCanvas.cpp
// ----------------------------------------------------------------------------
// WinAPI는 Child HWND와 메시지만 담당합니다. 실제 Preview 그리기는 게임과 같은
// Direct2D UIRenderer + UIWidget + UILayout을 사용합니다.
// ============================================================================
#include "Tools/UIDesigner/Canvas/UIDesignerCanvas.h"
#include "UI/Document/Serialization/UIUtf8.h"
#include "UI/UILayout.h"
#include "UI/UITheme.h"
#include "UI/UIWidget.h"
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace
{
    constexpr wchar_t kCanvasClass[]=L"DXFrameworkUIDesignerRuntimeCanvas";
    constexpr UIColor kCanvasBackground{.035f,.045f,.055f,1.f};
    constexpr UIColor kSelection{1.f,.78f,.29f,1.f};
    constexpr UIColor kColumnSelection{.36f,.70f,1.f,1.f};
}

UIDesignerCanvas::~UIDesignerCanvas(){Destroy();}

bool UIDesignerCanvas::RegisterClass(HINSTANCE instance)
{
    WNDCLASSEXW existing{};existing.cbSize=sizeof(existing);
    if(GetClassInfoExW(instance,kCanvasClass,&existing))return true;
    WNDCLASSEXW wc{};
    wc.cbSize=sizeof(wc);
    wc.style=CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS;
    wc.lpfnWndProc=WindowProc;
    wc.hInstance=instance;
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hbrBackground=nullptr;
    wc.lpszClassName=kCanvasClass;
    return RegisterClassExW(&wc)!=0;
}

bool UIDesignerCanvas::Create(HINSTANCE instance,HWND parent,int controlId,
                              SelectionChanged onSelection)
{
    Destroy();
    instance_=instance;
    onSelection_=std::move(onSelection);
    lastError_.clear();
    if(!RegisterClass(instance_))
    {lastError_=L"Designer Canvas Window Class 등록 실패";return false;}
    hwnd_=CreateWindowExW(WS_EX_CLIENTEDGE,kCanvasClass,L"",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_CLIPSIBLINGS,
        0,0,600,600,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
        instance_,this);
    if(!hwnd_)
    {lastError_=L"Designer Canvas Child Window 생성 실패";return false;}
    if(!renderer_.InitializeForWindow(hwnd_))
    {lastError_=renderer_.GetError();Destroy();return false;}
    return true;
}

void UIDesignerCanvas::Destroy()
{
    if(hwnd_ && IsWindow(hwnd_))DestroyWindow(hwnd_);
    hwnd_=nullptr;
    model_=nullptr;
    hits_.clear();
}

void UIDesignerCanvas::SetBounds(int x,int y,int width,int height)
{
    if(!hwnd_)return;
    const int safeW=std::max(1,width),safeH=std::max(1,height);
    MoveWindow(hwnd_,x,y,safeW,safeH,TRUE);
    renderer_.ResizeWindowTarget(static_cast<unsigned int>(safeW),static_cast<unsigned int>(safeH));
}
void UIDesignerCanvas::SetModel(const UIDesignerModel* model){model_=model;Invalidate();}
void UIDesignerCanvas::SetSelection(const UIDesignerPath& path){selection_=path;Invalidate();}
void UIDesignerCanvas::Invalidate(){if(hwnd_)InvalidateRect(hwnd_,nullptr,FALSE);}

LRESULT CALLBACK UIDesignerCanvas::WindowProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam)
{
    UIDesignerCanvas* self=nullptr;
    if(message==WM_NCCREATE)
    {
        auto* create=reinterpret_cast<CREATESTRUCTW*>(lParam);
        self=static_cast<UIDesignerCanvas*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        if(self)self->hwnd_=hwnd;
    }
    else self=reinterpret_cast<UIDesignerCanvas*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    return self?self->HandleMessage(message,wParam,lParam):DefWindowProcW(hwnd,message,wParam,lParam);
}

LRESULT UIDesignerCanvas::HandleMessage(UINT message,WPARAM wParam,LPARAM lParam)
{
    switch(message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps{};BeginPaint(hwnd_,&ps);Render();EndPaint(hwnd_,&ps);
        }
        return 0;
    case WM_SIZE:
        {
            const UINT width=LOWORD(lParam),height=HIWORD(lParam);
            if(width&&height)renderer_.ResizeWindowTarget(width,height);
        }
        return 0;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd_);SelectAt(static_cast<float>(GET_X_LPARAM(lParam)),
                                static_cast<float>(GET_Y_LPARAM(lParam)));return 0;
    case WM_ERASEBKGND:return 1;
    case WM_NCDESTROY:
        {
            HWND dying=hwnd_;
            SetWindowLongPtrW(dying,GWLP_USERDATA,0);
            hwnd_=nullptr;
            return DefWindowProcW(dying,message,wParam,lParam);
        }
    default:return DefWindowProcW(hwnd_,message,wParam,lParam);
    }
}

UIDesignerPath UIDesignerCanvas::PreviewPanelPath()const
{
    if(!model_)return {};
    if(const auto p=model_->FindAncestor(selection_,UIElementType::Panel);p.Valid)return p;
    return model_->FirstPanel();
}

float UIDesignerCanvas::FloatAttribute(const UIElementDefinition& e,const char* name,float fallback)
{
    try
    {
        const auto value=UIDesignerModel::Attribute(e,name);
        if(value.empty())return fallback;
        std::size_t used=0;const float parsed=std::stof(value,&used);
        return used==value.size()&&std::isfinite(parsed)?parsed:fallback;
    }
    catch(...){return fallback;}
}
bool UIDesignerCanvas::BoolAttribute(const UIElementDefinition& e,const char* name,bool fallback)
{
    const auto value=UIDesignerModel::Attribute(e,name);
    if(value.empty())return fallback;
    return value=="true"||value=="1"||value=="yes"||value=="on";
}
std::wstring UIDesignerCanvas::TextAttribute(const UIElementDefinition& e,const char* name,
                                              std::wstring fallback)
{
    const auto value=UIDesignerModel::Attribute(e,name);
    return value.empty()?std::move(fallback):UIUtf8::ToWide(value);
}

std::unique_ptr<UIWidget> UIDesignerCanvas::MakePreviewWidget(const UIElementDefinition& e)
{
    const auto text=TextAttribute(e,"text",UIUtf8::ToWide(UIDesignerModel::TypeName(e.Type)));
    if(e.Type==UIElementType::Label)return std::make_unique<UILabel>(text);
    if(e.Type==UIElementType::CheckBox)
    {
        const auto state=std::make_shared<bool>(BoolAttribute(e,"value",true));
        return std::make_unique<UICheckBox>(text,[state]{return *state;},[state](bool v){*state=v;});
    }
    if(e.Type==UIElementType::Slider)
    {
        float minimum=FloatAttribute(e,"min",0.f),maximum=FloatAttribute(e,"max",100.f);
        float step=FloatAttribute(e,"step",1.f);if(maximum<=minimum)maximum=minimum+1.f;if(step<=0.f)step=1.f;
        const auto state=std::make_shared<float>(std::clamp(FloatAttribute(e,"value",minimum),minimum,maximum));
        return std::make_unique<UISlider>(text,minimum,maximum,step,[state]{return *state;},
                                         [state](float v){*state=v;});
    }
    if(e.Type==UIElementType::Button)return std::make_unique<UIButton>(text,[]{});
    return {};
}

std::vector<UIDesignerCanvas::PreviewItem> UIDesignerCanvas::CollectColumn(
    const UIElementDefinition& column,const UIDesignerPath& columnPath)const
{
    std::vector<PreviewItem> result;
    for(std::size_t i=0;i<column.Children.size();++i)
    {
        const auto& child=column.Children[i];
        if(child.Type==UIElementType::Panel||child.Type==UIElementType::Column||
           child.Type==UIElementType::Document)continue;
        UIDesignerPath path=columnPath;path.Valid=true;path.Indices.push_back(i);
        result.push_back({&child,std::move(path)});
    }
    return result;
}

void UIDesignerCanvas::Render()
{
    if(!hwnd_ || !renderer_.BeginWindow())return;
    RECT client{};GetClientRect(hwnd_,&client);
    const float cw=static_cast<float>(std::max<LONG>(1,client.right-client.left));
    const float ch=static_cast<float>(std::max<LONG>(1,client.bottom-client.top));
    renderer_.FillRect({0.f,0.f,cw,ch},kCanvasBackground);
    hits_.clear();

    const auto panelPath=PreviewPanelPath();
    const auto* panel=model_?model_->Resolve(panelPath):nullptr;
    if(!panel)
    {
        renderer_.Text(L"Hierarchy에서 Panel을 추가하세요.",{20.f,20.f,cw-40.f,ch-40.f},
                       UITheme::Muted,UIFont::Heading);
        renderer_.End();return;
    }

    const float panelW=std::max(120.f,FloatAttribute(*panel,"width",420.f));
    const float panelH=std::max(90.f,FloatAttribute(*panel,"height",420.f));
    // Runtime과 동일한 1:1 Pixel Preview. Canvas보다 큰 Panel은 좌상단부터 보여주며
    // 이후 Designer Canvas Pan/Zoom 단계에서 탐색 기능을 확장할 수 있습니다.
    const float panelX=panelW+40.f<=cw?(cw-panelW)*.5f:20.f;
    const float usableH=std::max(1.f,ch-38.f);
    const float panelY=panelH+30.f<=usableH?(usableH-panelH)*.5f:15.f;
    const UIRect panelRect{panelX,panelY,panelW,panelH};
    UIPanel previewPanel;previewPanel.SetBounds(panelRect);
    previewPanel.Render(renderer_,TextAttribute(*panel,"title",L"UI Panel"));
    hits_.push_back({panelRect,panelPath});

    struct ColumnData{UIDesignerPath Path;std::vector<PreviewItem> Items;};
    std::vector<ColumnData> columns;
    std::vector<PreviewItem> direct;
    for(std::size_t i=0;i<panel->Children.size();++i)
    {
        const auto& child=panel->Children[i];
        UIDesignerPath childPath=panelPath;childPath.Valid=true;childPath.Indices.push_back(i);
        if(child.Type==UIElementType::Column)columns.push_back({childPath,CollectColumn(child,childPath)});
        else if(child.Type!=UIElementType::Panel&&child.Type!=UIElementType::Document)
            direct.push_back({&child,childPath});
    }
    if(columns.empty())columns.push_back({panelPath,std::move(direct)});
    else if(!direct.empty())
        columns.front().Items.insert(columns.front().Items.end(),direct.begin(),direct.end());

    // Runtime UIManager와 동일하게 첫 번째 열 + 나머지는 두 번째 열에 이어 붙입니다.
    std::vector<PreviewItem> flat=columns.front().Items;
    std::size_t rightStart=flat.size();
    for(std::size_t c=1;c<columns.size();++c)
        flat.insert(flat.end(),columns[c].Items.begin(),columns[c].Items.end());
    if(columns.size()==1)rightStart=flat.size();

    std::vector<std::unique_ptr<UIWidget>> widgets;
    std::vector<float> heights;widgets.reserve(flat.size());heights.reserve(flat.size());
    for(const auto& item:flat)
    {
        auto widget=MakePreviewWidget(*item.Element);
        if(!widget)continue;
        heights.push_back(widget->PreferredHeight());widgets.push_back(std::move(widget));
    }
    // 현재 정의된 Element만 Widget으로 변환되므로 flat / widgets 개수는 동일합니다.
    const auto layout=UILayout::Arrange(panelRect,heights,rightStart);
    renderer_.PushClip(layout.ContentClip);

    // 큰 Column 영역을 먼저 Hit 등록하고, 그 뒤 실제 Widget을 등록합니다.
    // SelectAt()은 뒤에서부터 검사하므로 Widget이 Column보다 우선 선택됩니다.
    const bool twoColumns=rightStart<heights.size();
    const float left=panelRect.X+UITheme::PanelMargin;
    const float innerW=panelRect.W-UITheme::PanelMargin*2.f;
    const float colW=twoColumns?std::max(0.f,(innerW-UITheme::ColumnGap)/2.f):innerW;
    for(std::size_t c=0;c<std::min<std::size_t>(2u,columns.size());++c)
    {
        const UIRect r{left+(c?colW+UITheme::ColumnGap:0.f),
                       panelRect.Y+UITheme::PanelContentTop,colW,
                       std::max(0.f,panelRect.H-UITheme::PanelContentTop-8.f)};
        hits_.push_back({r,columns[c].Path});
        if(selection_==columns[c].Path)renderer_.StrokeRect(r,kColumnSelection,1.f);
    }

    for(std::size_t i=0;i<widgets.size()&&i<layout.Items.size()&&i<flat.size();++i)
    {
        widgets[i]->SetBounds(layout.Items[i]);
        widgets[i]->Render(renderer_,false);
        hits_.push_back({layout.Items[i],flat[i].Path});
        if(selection_==flat[i].Path)renderer_.StrokeRect(layout.Items[i],kSelection,2.f);
    }
    renderer_.PopClip();

    if(selection_==panelPath)renderer_.StrokeRect(panelRect,kSelection,2.f);
    renderer_.Text(L"Canvas: Runtime UIRenderer / UIWidget / UILayout 실제 Preview",
                   {12.f,ch-30.f,cw-24.f,22.f},UITheme::Muted,UIFont::Small);
    renderer_.End();
}

void UIDesignerCanvas::SelectAt(float x,float y)
{
    for(auto it=hits_.rbegin();it!=hits_.rend();++it)
    {
        if(it->Rect.Contains(x,y))
        {
            selection_=it->Path;
            if(onSelection_)onSelection_(selection_);
            Invalidate();return;
        }
    }
}
