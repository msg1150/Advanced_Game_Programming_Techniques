#include "UI/Document/UIWidgetFactory.h"
#include "UI/Document/UIBindingRegistry.h"
#include "UI/Document/UIUtf8.h"
#include "UI/UIManager.h"
#include <algorithm>
#include <memory>
#include <string>
#include <stdexcept>
#include <utility>
namespace
{
const std::string& EmptyString(){static const std::string value;return value;}
const std::string& Attr(const UIElementDefinition& e,std::string_view name)
{if(const auto* value=e.Find(name))return *value;return EmptyString();}
std::string AttrOr(const UIElementDefinition& e,std::string_view name,std::string fallback)
{if(const auto* value=e.Find(name))return *value;return fallback;}
float FloatOr(const UIElementDefinition& e,std::string_view name,float fallback,
              std::vector<UIDiagnostic>& diagnostics)
{
    const auto* value=e.Find(name);if(!value)return fallback;
    try
    {
        std::size_t used=0;const float result=std::stof(*value,&used);
        if(used!=value->size())throw std::invalid_argument("trailing");
        return result;
    }
    catch(...)
    {
        diagnostics.push_back({UIDiagnosticSeverity::Warning,e.Line,e.Column,
            std::string(name)+" 숫자를 읽지 못해 기본값을 사용합니다: "+*value});
        return fallback;
    }
}
bool BoolOr(const UIElementDefinition& e,std::string_view name,bool fallback)
{
    if(const auto* value=e.Find(name))
        return *value=="true"||*value=="1"||*value=="yes"||*value=="on";
    return fallback;
}
void AddId(UIDocumentInstance& instance,const UIElementDefinition& e,UIWidget* widget,
           std::vector<UIDiagnostic>& diagnostics)
{
    const auto& id=Attr(e,"id");if(id.empty())return;
    if(!instance.Widgets.emplace(id,widget).second)
        diagnostics.push_back({UIDiagnosticSeverity::Warning,e.Line,e.Column,"중복 Widget id: "+id});
}
std::unique_ptr<UIWidget> MakeWidget(const UIElementDefinition& e,const UIBindingRegistry& bindings,
                                     std::vector<UIDiagnostic>& diagnostics)
{
    const std::wstring text=UIUtf8::ToWide(AttrOr(e,"text",e.Tag));
    if(e.Type==UIElementType::Label)
    {
        const auto& binding=Attr(e,"textBinding");
        if(!binding.empty())
        {
            if(const auto* getter=bindings.FindText(binding))return std::make_unique<UILabel>(*getter);
            diagnostics.push_back({UIDiagnosticSeverity::Warning,e.Line,e.Column,
                "Text Binding을 찾지 못했습니다: "+binding});
        }
        return std::make_unique<UILabel>(text);
    }
    if(e.Type==UIElementType::CheckBox)
    {
        const auto& name=Attr(e,"binding");
        if(!name.empty())
        {
            if(const auto* binding=bindings.FindBool(name))
                return std::make_unique<UICheckBox>(text,binding->Get,binding->Set);
            diagnostics.push_back({UIDiagnosticSeverity::Warning,e.Line,e.Column,
                "Bool Binding을 찾지 못해 문서 Local 값을 사용합니다: "+name});
        }
        const auto state=std::make_shared<bool>(BoolOr(e,"value",false));
        return std::make_unique<UICheckBox>(text,[state]{return *state;},[state](bool value){*state=value;});
    }
    if(e.Type==UIElementType::Slider)
    {
        float minimum=FloatOr(e,"min",0.f,diagnostics);
        float maximum=FloatOr(e,"max",100.f,diagnostics);
        float step=FloatOr(e,"step",1.f,diagnostics);
        if(maximum<=minimum){maximum=minimum+1.f;diagnostics.push_back({UIDiagnosticSeverity::Warning,e.Line,e.Column,"Slider max는 min보다 커야 하므로 자동 보정했습니다."});}
        if(step<=0.f){step=1.f;diagnostics.push_back({UIDiagnosticSeverity::Warning,e.Line,e.Column,"Slider step은 0보다 커야 하므로 1을 사용합니다."});}
        const auto& name=Attr(e,"binding");
        if(!name.empty())
        {
            if(const auto* binding=bindings.FindFloat(name))
                return std::make_unique<UISlider>(text,minimum,maximum,step,binding->Get,binding->Set);
            diagnostics.push_back({UIDiagnosticSeverity::Warning,e.Line,e.Column,
                "Float Binding을 찾지 못해 문서 Local 값을 사용합니다: "+name});
        }
        const auto state=std::make_shared<float>(std::clamp(FloatOr(e,"value",minimum,diagnostics),minimum,maximum));
        return std::make_unique<UISlider>(text,minimum,maximum,step,[state]{return *state;},
            [state,minimum,maximum](float value){*state=std::clamp(value,minimum,maximum);});
    }
    if(e.Type==UIElementType::Button)
    {
        const auto& name=Attr(e,"action");
        if(!name.empty())
        {
            if(const auto* action=bindings.FindAction(name))return std::make_unique<UIButton>(text,*action);
            diagnostics.push_back({UIDiagnosticSeverity::Warning,e.Line,e.Column,
                "Action Binding을 찾지 못해 빈 Button으로 생성합니다: "+name});
        }
        return std::make_unique<UIButton>(text,[]{});
    }
    return {};
}
void AddColumn(UIManager& ui,UIManager::PanelId panel,const UIElementDefinition& column,
               const UIBindingRegistry& bindings,UIDocumentInstance& instance,
               std::vector<UIDiagnostic>& diagnostics)
{
    for(const auto& child:column.Children)
    {
        if(child.Type==UIElementType::Column||child.Type==UIElementType::Panel||child.Type==UIElementType::Document)
        {diagnostics.push_back({UIDiagnosticSeverity::Warning,child.Line,child.Column,"현재 v0.4에서는 Column 안의 중첩 Container를 지원하지 않습니다: "+child.Tag});continue;}
        auto widget=MakeWidget(child,bindings,diagnostics);
        if(!widget)
        {diagnostics.push_back({UIDiagnosticSeverity::Warning,child.Line,child.Column,"지원하지 않는 Widget Tag: "+child.Tag});continue;}
        UIWidget* raw=widget.get();AddId(instance,child,raw,diagnostics);ui.AddToPanel(panel,std::move(widget));
    }
}
}
bool UIWidgetFactory::Build(UIManager& ui,const UIDocumentDefinition& document,
                            const UIBindingRegistry& bindings,UIDocumentInstance& instance,
                            std::vector<UIDiagnostic>& diagnostics)
{
    instance=UIDocumentInstance{};
    if(document.Root.Type!=UIElementType::Document)
    {diagnostics.push_back({UIDiagnosticSeverity::Error,1u,1u,"UIDocument Root가 필요합니다."});return false;}
    if(document.Version!=1u)
        diagnostics.push_back({UIDiagnosticSeverity::Warning,document.Root.Line,document.Root.Column,
            "현재 Runtime은 UIDocument version=1을 기준으로 동작합니다."});
    std::size_t generatedPanelIndex=0u;
    for(const auto& panelDef:document.Root.Children)
    {
        if(panelDef.Type!=UIElementType::Panel)
        {diagnostics.push_back({UIDiagnosticSeverity::Warning,panelDef.Line,panelDef.Column,"UIDocument의 직접 자식은 Panel만 지원합니다: "+panelDef.Tag});continue;}
        const auto title=UIUtf8::ToWide(AttrOr(panelDef,"title","UI Panel"));
        const auto open=UIUtf8::ToWide(AttrOr(panelDef,"openLabel","UI 열기"));
        const auto close=UIUtf8::ToWide(AttrOr(panelDef,"closeLabel","UI 닫기"));
        const float width=FloatOr(panelDef,"width",360.f,diagnostics);
        const float height=FloatOr(panelDef,"height",360.f,diagnostics);
        const auto panel=ui.CreatePanel(title,open,close,std::max(120.f,width),std::max(90.f,height));
        std::string panelId=AttrOr(panelDef,"id","Panel"+std::to_string(generatedPanelIndex++));
        if(!instance.Panels.emplace(panelId,panel).second)
            diagnostics.push_back({UIDiagnosticSeverity::Warning,panelDef.Line,panelDef.Column,"중복 Panel id: "+panelId});

        bool second=false;bool sawColumn=false;
        for(const auto& child:panelDef.Children)
        {
            if(child.Type==UIElementType::Column)
            {
                if(!sawColumn){sawColumn=true;AddColumn(ui,panel,child,bindings,instance,diagnostics);}
                else
                {
                    if(!second){ui.StartSecondColumn(panel);second=true;}
                    else diagnostics.push_back({UIDiagnosticSeverity::Warning,child.Line,child.Column,"현재 Panel은 최대 2개의 Column만 배치합니다. 추가 Column은 두 번째 열에 이어 붙입니다."});
                    AddColumn(ui,panel,child,bindings,instance,diagnostics);
                }
            }
            else
            {
                // 간단한 문서는 Column 없이 Widget을 Panel 바로 아래 둘 수 있습니다.
                UIElementDefinition pseudo;pseudo.Type=UIElementType::Column;pseudo.Children.push_back(child);
                AddColumn(ui,panel,pseudo,bindings,instance,diagnostics);
            }
        }
    }
    if(instance.Panels.empty())
    {diagnostics.push_back({UIDiagnosticSeverity::Error,document.Root.Line,document.Root.Column,"생성할 Panel이 없습니다."});return false;}
    return true;
}
