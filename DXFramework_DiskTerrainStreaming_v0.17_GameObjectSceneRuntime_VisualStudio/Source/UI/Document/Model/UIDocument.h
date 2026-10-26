// ============================================================================
// UIDocument.h
// ----------------------------------------------------------------------------
// Runtime과 차후 UI Designer가 함께 사용할 UI 문서의 중간 표현(IR)입니다.
// XML Parser는 문자열을 이 Tree로 만들고, WidgetFactory는 이 Tree만 읽습니다.
// Parser와 Runtime Widget 생성을 분리하여 파일 포맷을 바꿔도 Widget 코드를 보호합니다.
// ============================================================================
#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
class UIWidget;

enum class UIElementType
{
    Document,Panel,Column,Label,CheckBox,Slider,Button,Unknown
};
struct UIAttribute
{
    std::string Name;
    std::string Value;
};
struct UIElementDefinition
{
    UIElementType Type=UIElementType::Unknown;
    std::string Tag;
    std::vector<UIAttribute> Attributes;
    std::vector<UIElementDefinition> Children;
    std::size_t Line=1u;
    std::size_t Column=1u;

    const std::string* Find(std::string_view name)const
    {
        for(const auto& attribute:Attributes)
            if(attribute.Name==name)return &attribute.Value;
        return nullptr;
    }
};
struct UIDocumentDefinition
{
    unsigned int Version=1u;
    UIElementDefinition Root;
};
enum class UIDiagnosticSeverity {Warning,Error};
struct UIDiagnostic
{
    UIDiagnosticSeverity Severity=UIDiagnosticSeverity::Error;
    std::size_t Line=1u;
    std::size_t Column=1u;
    std::string Message;
};
// 생성된 Runtime 객체의 ID Index입니다. 소유권은 UIManager에 있고 이 구조는 찾기용입니다.
struct UIDocumentInstance
{
    std::unordered_map<std::string,std::size_t> Panels;
    std::unordered_map<std::string,UIWidget*> Widgets;
};
