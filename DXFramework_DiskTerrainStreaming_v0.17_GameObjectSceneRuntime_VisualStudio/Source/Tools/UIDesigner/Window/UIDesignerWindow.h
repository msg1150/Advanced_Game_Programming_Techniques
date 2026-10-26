// ============================================================================
// UIDesignerWindow.h
// ----------------------------------------------------------------------------
// 게임의 DirectX BackBuffer와 분리된 별도 Top-Level Win32 UI Designer 창입니다.
// 메인 창의 UI Designer 버튼에서 Show()만 호출하며, Designer 창을 닫아도
// 게임의 WM_QUIT을 발생시키지 않습니다.
// ============================================================================
#pragma once
#include "Tools/UIDesigner/Core/UIDesignerModel.h"
#include "Tools/UIDesigner/Canvas/UIDesignerCanvas.h"
#include <Windows.h>
#include <filesystem>
#include <string>
#include <vector>

class UIDesignerWindow
{
public:
    UIDesignerWindow()=default;
    ~UIDesignerWindow();
    UIDesignerWindow(const UIDesignerWindow&)=delete;
    UIDesignerWindow& operator=(const UIDesignerWindow&)=delete;

    bool Initialize(HINSTANCE instance,std::filesystem::path assetDirectory,
                    std::filesystem::path initialDocument={});
    void Show();
    bool IsOpen()const noexcept{return hwnd_!=nullptr;}
    const std::wstring& GetLastError()const{return lastError_;}

private:

    static LRESULT CALLBACK WindowProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);
    LRESULT HandleMessage(UINT message,WPARAM wParam,LPARAM lParam);

    bool RegisterWindowClasses();
    bool CreateDesignerWindow();
    void CreateControls();
    void LayoutControls(int width,int height);
    HWND CreateLabel(const wchar_t* text,int id=0);
    HWND CreateEdit(int id);
    void ApplyFont(HWND control)const;

    void NewDocument();
    void OpenDocument();
    bool SaveDocument();
    bool SaveDocumentAs();
    bool ConfirmDiscardChanges();
    void AddPaletteSelection();
    void DeleteSelection();
    void MoveSelection(int direction);
    void ApplyInspector();
    void OnHierarchySelectionChanged();

    void RefreshAll();
    void RefreshHierarchy();
    void RefreshInspector();
    void UpdateWindowTitle();
    void SetStatus(std::wstring text);
    void MarkDirty();
    void SelectPath(const UIDesignerPath& path);

    static std::wstring Wide(std::string_view utf8);
    static std::string Utf8(std::wstring_view wide);
    static std::wstring EditText(HWND edit);
    static void SetEditText(HWND edit,const std::wstring& value);
    static bool TryParsePositive(const std::wstring& text,float minimum,float& value);

    HINSTANCE instance_=nullptr;
    HWND hwnd_=nullptr;
    UIDesignerCanvas canvas_;
    HWND paletteList_=nullptr;
    HWND hierarchyList_=nullptr;
    HWND status_=nullptr;
    HWND typeValue_=nullptr;
    HWND idEdit_=nullptr;
    HWND textEdit_=nullptr;
    HWND bindingEdit_=nullptr;
    HWND minEdit_=nullptr;
    HWND maxEdit_=nullptr;
    HWND stepEdit_=nullptr;
    HWND widthEdit_=nullptr;
    HWND heightEdit_=nullptr;
    HWND openLabelEdit_=nullptr;
    HWND closeLabelEdit_=nullptr;
    HFONT font_=nullptr;

    UIDesignerModel model_;
    UIDesignerPath selection_;
    std::vector<UIDesignerHierarchyItem> hierarchyItems_;
    std::filesystem::path assetDirectory_;
    std::filesystem::path initialDocument_;
    bool initialLoaded_=false;
    bool dirty_=false;
    std::wstring lastError_;
};
