// ============================================================================
// UIWidgetFactory.h
// ----------------------------------------------------------------------------
// UIDocument의 declarative Element를 기존 Retained Widget으로 변환합니다.
// UIManager/Widget은 XML을 알지 않고, Parser는 Runtime Widget을 알지 않습니다.
// ============================================================================
#pragma once
#include "UI/Document/Model/UIDocument.h"
#include <vector>
class UIBindingRegistry;
class UIManager;
class UIWidgetFactory
{
public:
    static bool Build(UIManager& ui,const UIDocumentDefinition& document,
                      const UIBindingRegistry& bindings,UIDocumentInstance& instance,
                      std::vector<UIDiagnostic>& diagnostics);
};
