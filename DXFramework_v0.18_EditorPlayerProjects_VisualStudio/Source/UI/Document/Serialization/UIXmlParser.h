// ============================================================================
// UIXmlParser.h : 자체 UI XML의 Syntax Parser.
// ----------------------------------------------------------------------------
// 범용 Web XML Parser를 목표로 하지 않습니다. XML 선언, Comment, Attribute,
// Self-closing/Nested Element, 기본 Entity를 안전하게 읽어 UIDocument IR로 변환합니다.
// ============================================================================
#pragma once
#include "UI/Document/Model/UIDocument.h"
#include <filesystem>
#include <string_view>
#include <vector>
class UIXmlParser
{
public:
    static bool Parse(std::string_view utf8,UIDocumentDefinition& out,
                      std::vector<UIDiagnostic>& diagnostics);
    static bool ParseFile(const std::filesystem::path& path,UIDocumentDefinition& out,
                          std::vector<UIDiagnostic>& diagnostics);
};
