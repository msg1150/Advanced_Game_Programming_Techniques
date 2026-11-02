// ============================================================================
// UIDesignerCanvas.h
// ----------------------------------------------------------------------------
// Designer 중앙 Canvas 전용 모듈입니다.
// 외부 Editor Shell은 WinAPI Control을 사용하지만, Canvas Preview 자체는
// 게임 Runtime과 같은 UIRenderer / UIWidget / UILayout / UITheme을 사용합니다.
// 따라서 Designer Preview와 실제 Runtime 외형/높이 계산이 따로 놀지 않습니다.
// ============================================================================
#pragma once
#include "Tools/UIDesigner/Core/UIDesignerModel.h"
#include "UI/UIRenderer.h"
#include "UI/UIWidget.h"
#include <Windows.h>
#include <functional>
#include <memory>
#include <vector>

class UIDesignerCanvas
{
public:
    using SelectionChanged=std::function<void(const UIDesignerPath&)>;

    UIDesignerCanvas()=default;
    ~UIDesignerCanvas();
    UIDesignerCanvas(const UIDesignerCanvas&)=delete;
    UIDesignerCanvas& operator=(const UIDesignerCanvas&)=delete;

    bool Create(HINSTANCE instance,HWND parent,int controlId,SelectionChanged onSelection);
    void Destroy();
    void SetBounds(int x,int y,int width,int height);
    void SetModel(const UIDesignerModel* model);
    void SetSelection(const UIDesignerPath& path);
    void Invalidate();
    HWND Handle()const noexcept{return hwnd_;}
    const std::wstring& GetLastError()const{return lastError_;}

private:
    struct HitItem
    {
        UIRect Rect{};
        UIDesignerPath Path;
    };
    struct PreviewItem
    {
        const UIElementDefinition* Element=nullptr;
        UIDesignerPath Path;
    };

    static LRESULT CALLBACK WindowProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);
    LRESULT HandleMessage(UINT message,WPARAM wParam,LPARAM lParam);
    static bool RegisterClass(HINSTANCE instance);

    void Render();
    void SelectAt(float x,float y);
    UIDesignerPath PreviewPanelPath()const;
    std::vector<PreviewItem> CollectColumn(const UIElementDefinition& column,
                                           const UIDesignerPath& columnPath)const;
    static std::unique_ptr<UIWidget> MakePreviewWidget(const UIElementDefinition& element);
    static float FloatAttribute(const UIElementDefinition& element,const char* name,float fallback);
    static bool BoolAttribute(const UIElementDefinition& element,const char* name,bool fallback);
    static std::wstring TextAttribute(const UIElementDefinition& element,const char* name,
                                      std::wstring fallback={});

    HINSTANCE instance_=nullptr;
    HWND hwnd_=nullptr;
    UIRenderer renderer_;
    const UIDesignerModel* model_=nullptr;
    UIDesignerPath selection_;
    SelectionChanged onSelection_;
    std::vector<HitItem> hits_;
    std::wstring lastError_;
};
