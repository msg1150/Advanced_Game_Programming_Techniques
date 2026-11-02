// ============================================================================
// UIXmlWriter.h
// ----------------------------------------------------------------------------
// 차후 Designer가 Runtime과 같은 포맷을 저장할 수 있도록 UIDocument IR -> XML을 제공합니다.
// 현재 v0.4에는 시각적 Designer가 없지만 저장 포맷을 Runtime Loader와 함께 먼저 고정합니다.
// ============================================================================
#pragma once
#include "UI/Document/Model/UIDocument.h"
#include <filesystem>
#include <string>
class UIXmlWriter
{
public:
    static std::string Write(const UIDocumentDefinition& document);
    static bool WriteFile(const std::filesystem::path& path,const UIDocumentDefinition& document);
};
