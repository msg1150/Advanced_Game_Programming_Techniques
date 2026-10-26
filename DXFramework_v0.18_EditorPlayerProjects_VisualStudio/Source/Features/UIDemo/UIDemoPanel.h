// ============================================================================
// UIDemoPanel.h
// ----------------------------------------------------------------------------
// Terrain과 독립적인 UIDocument/Binding 검증 Feature.
// XML이 없거나 Syntax Error일 때는 기존 Code-driven UI로 Fallback하여 게임 실행을 보호합니다.
// ============================================================================
#pragma once
#include "UI/Document/Model/UIBindingRegistry.h"
#include "UI/Document/Model/UIDocument.h"
#include <filesystem>
#include <string>
#include <vector>
class UIManager;
class UIDemoPanel
{
public:
    void Register(UIManager& ui,const std::filesystem::path& documentPath=L"Assets/UI/UIDemo.ui.xml");
private:
    void RegisterBindings();
    void RegisterFallback(UIManager& ui);
    std::wstring ClickText()const;
    bool checkbox_=false;
    float sliderValue_=50.f;
    unsigned int clicks_=0;
    UIBindingRegistry bindings_;
    UIDocumentInstance documentInstance_;
    std::vector<UIDiagnostic> diagnostics_;
};
