// ============================================================================
// UIDocumentLoader.h
// ----------------------------------------------------------------------------
// File/String -> XML Parse -> UIDocument IR -> WidgetFactory의 Runtime 진입점입니다.
// Designer도 같은 Parser/Writer를 사용하게 하여 Editor 전용 포맷이 생기지 않게 합니다.
// ============================================================================
#pragma once
#include "UI/Document/Model/UIDocument.h"
#include <filesystem>
#include <string_view>
#include <vector>
class UIBindingRegistry;
class UIManager;
struct UIDocumentLoadResult
{
    bool Success=false;
    UIDocumentDefinition Definition;
    UIDocumentInstance Instance;
    std::vector<UIDiagnostic> Diagnostics;
};
class UIDocumentLoader
{
public:
    static UIDocumentLoadResult LoadFile(UIManager& ui,const UIBindingRegistry& bindings,
                                         const std::filesystem::path& path);
    static UIDocumentLoadResult LoadString(UIManager& ui,const UIBindingRegistry& bindings,
                                           std::string_view utf8);
};
