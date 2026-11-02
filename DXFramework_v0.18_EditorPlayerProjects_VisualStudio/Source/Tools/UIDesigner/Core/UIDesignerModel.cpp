// ============================================================================
// UIDesignerModel.cpp
// ============================================================================
#include "Tools/UIDesigner/Core/UIDesignerModel.h"
#include "UI/Document/Serialization/UIXmlParser.h"
#include "UI/Document/Serialization/UIXmlWriter.h"
#include <algorithm>
#include <utility>

namespace
{
UIElementDefinition MakeElement(UIElementType type,const char* tag)
{
    UIElementDefinition e;
    e.Type=type;
    e.Tag=tag;
    e.Line=1u;
    e.Column=1u;
    return e;
}
}

UIDesignerModel::UIDesignerModel(){NewDocument();}

void UIDesignerModel::NewDocument()
{
    currentPath_.clear();
    document_=UIDocumentDefinition{};
    document_.Version=1u;
    document_.Root=MakeElement(UIElementType::Document,"UIDocument");
    document_.Root.Attributes.push_back({"version","1"});
    AddPanel();
}

bool UIDesignerModel::Load(const std::filesystem::path& path,
                           std::vector<UIDiagnostic>& diagnostics)
{
    UIDocumentDefinition loaded;
    diagnostics.clear();
    if(!UIXmlParser::ParseFile(path,loaded,diagnostics))return false;
    document_=std::move(loaded);
    currentPath_=path;
    return true;
}

bool UIDesignerModel::Save(const std::filesystem::path& path)const
{
    std::error_code ec;
    if(path.has_parent_path())std::filesystem::create_directories(path.parent_path(),ec);
    return UIXmlWriter::WriteFile(path,document_);
}

const UIElementDefinition* UIDesignerModel::Resolve(const UIDesignerPath& path)const
{
    if(!path.Valid)return nullptr;
    const UIElementDefinition* node=&document_.Root;
    for(const std::size_t index:path.Indices)
    {
        if(index>=node->Children.size())return nullptr;
        node=&node->Children[index];
    }
    return node;
}

UIElementDefinition* UIDesignerModel::Resolve(const UIDesignerPath& path)
{
    if(!path.Valid)return nullptr;
    UIElementDefinition* node=&document_.Root;
    for(const std::size_t index:path.Indices)
    {
        if(index>=node->Children.size())return nullptr;
        node=&node->Children[index];
    }
    return node;
}

UIDesignerPath UIDesignerModel::ParentOf(const UIDesignerPath& path)const
{
    if(!path.Valid || path.Indices.empty())return {};
    UIDesignerPath parent=path;
    parent.Indices.pop_back();
    if(parent.Indices.empty())parent.Valid=false; // Root는 Inspector 선택 대상으로 사용하지 않는다.
    return parent;
}

UIDesignerPath UIDesignerModel::FindAncestor(const UIDesignerPath& path,UIElementType type)const
{
    if(!path.Valid)return {};
    UIDesignerPath cursor=path;
    while(cursor.Valid)
    {
        const auto* element=Resolve(cursor);
        if(element && element->Type==type)return cursor;
        if(cursor.Indices.empty())break;
        cursor.Indices.pop_back();
        if(cursor.Indices.empty())cursor.Valid=false;
    }
    return {};
}

UIDesignerPath UIDesignerModel::FirstPanel()const
{
    for(std::size_t i=0;i<document_.Root.Children.size();++i)
        if(document_.Root.Children[i].Type==UIElementType::Panel)
            return {true,{i}};
    return {};
}

std::string UIDesignerModel::Attribute(const UIElementDefinition& element,const char* name,
                                       std::string fallback)
{
    if(const auto* value=element.Find(name))return *value;
    return fallback;
}

void UIDesignerModel::SetAttribute(UIElementDefinition& element,std::string name,std::string value,
                                   bool removeWhenEmpty)
{
    if(removeWhenEmpty && value.empty())
    {
        RemoveAttribute(element,name.c_str());
        return;
    }
    for(auto& attribute:element.Attributes)
        if(attribute.Name==name){attribute.Value=std::move(value);return;}
    element.Attributes.push_back({std::move(name),std::move(value)});
}

void UIDesignerModel::RemoveAttribute(UIElementDefinition& element,const char* name)
{
    element.Attributes.erase(
        std::remove_if(element.Attributes.begin(),element.Attributes.end(),
            [name](const UIAttribute& a){return a.Name==name;}),
        element.Attributes.end());
}

const char* UIDesignerModel::TypeName(UIElementType type)noexcept
{
    switch(type)
    {
    case UIElementType::Panel:return "Panel";
    case UIElementType::Column:return "Column";
    case UIElementType::Label:return "Label";
    case UIElementType::CheckBox:return "CheckBox";
    case UIElementType::Slider:return "Slider";
    case UIElementType::Button:return "Button";
    case UIElementType::Document:return "UIDocument";
    default:return "Unknown";
    }
}

bool UIDesignerModel::IdExists(const UIElementDefinition& element,const std::string& id)const
{
    if(const auto* value=element.Find("id");value && *value==id)return true;
    for(const auto& child:element.Children)
        if(IdExists(child,id))return true;
    return false;
}

std::string UIDesignerModel::UniqueId(const char* prefix)const
{
    for(unsigned int n=1u;n<100000u;++n)
    {
        std::string candidate=std::string(prefix)+std::to_string(n);
        if(!IdExists(document_.Root,candidate))return candidate;
    }
    return std::string(prefix)+"New";
}

UIDesignerPath UIDesignerModel::AddPanel()
{
    UIElementDefinition panel=MakeElement(UIElementType::Panel,"Panel");
    panel.Attributes={{"id",UniqueId("Panel")},{"title","새 UI Panel"},
                      {"openLabel","UI 열기"},{"closeLabel","UI 닫기"},
                      {"width","420"},{"height","420"}};
    panel.Children.push_back(MakeElement(UIElementType::Column,"Column"));
    document_.Root.Children.push_back(std::move(panel));
    return {true,{document_.Root.Children.size()-1u}};
}

UIDesignerPath UIDesignerModel::TargetPanel(const UIDesignerPath& context)const
{
    if(const auto panel=FindAncestor(context,UIElementType::Panel);panel.Valid)return panel;
    return FirstPanel();
}

UIDesignerPath UIDesignerModel::AddColumn(const UIDesignerPath& panelPath)
{
    auto* panel=Resolve(panelPath);
    if(!panel || panel->Type!=UIElementType::Panel)return {};
    panel->Children.push_back(MakeElement(UIElementType::Column,"Column"));
    UIDesignerPath result=panelPath;
    result.Indices.push_back(panel->Children.size()-1u);
    result.Valid=true;
    return result;
}

UIDesignerPath UIDesignerModel::TargetColumn(const UIDesignerPath& context)
{
    if(const auto column=FindAncestor(context,UIElementType::Column);column.Valid)return column;
    auto panelPath=TargetPanel(context);
    if(!panelPath.Valid)panelPath=AddPanel();
    auto* panel=Resolve(panelPath);
    if(!panel)return {};
    for(std::size_t i=0;i<panel->Children.size();++i)
        if(panel->Children[i].Type==UIElementType::Column)
        {
            UIDesignerPath result=panelPath;
            result.Indices.push_back(i);
            result.Valid=true;
            return result;
        }
    return AddColumn(panelPath);
}

UIDesignerPath UIDesignerModel::AddElement(UIElementType type,const UIDesignerPath& context)
{
    if(type==UIElementType::Panel)return AddPanel();
    if(type==UIElementType::Column)
    {
        auto panel=TargetPanel(context);
        if(!panel.Valid)panel=AddPanel();
        return AddColumn(panel);
    }

    const auto columnPath=TargetColumn(context);
    auto* column=Resolve(columnPath);
    if(!column)return {};

    UIElementDefinition item;
    switch(type)
    {
    case UIElementType::Label:
        item=MakeElement(type,"Label");
        item.Attributes={{"id",UniqueId("Label")},{"text","새 Label"}};
        break;
    case UIElementType::CheckBox:
        item=MakeElement(type,"CheckBox");
        item.Attributes={{"id",UniqueId("CheckBox")},{"text","새 CheckBox"},{"value","false"}};
        break;
    case UIElementType::Slider:
        item=MakeElement(type,"Slider");
        item.Attributes={{"id",UniqueId("Slider")},{"text","새 Slider"},
                         {"min","0"},{"max","100"},{"step","1"},{"value","50"}};
        break;
    case UIElementType::Button:
        item=MakeElement(type,"Button");
        item.Attributes={{"id",UniqueId("Button")},{"text","새 Button"}};
        break;
    default:return {};
    }
    column->Children.push_back(std::move(item));
    UIDesignerPath result=columnPath;
    result.Indices.push_back(column->Children.size()-1u);
    result.Valid=true;
    return result;
}

bool UIDesignerModel::Remove(const UIDesignerPath& path,UIDesignerPath& nextSelection)
{
    nextSelection={};
    if(!path.Valid || path.Indices.empty())return false;
    UIDesignerPath parentPath=path;
    const std::size_t removeIndex=parentPath.Indices.back();
    parentPath.Indices.pop_back();

    UIElementDefinition* parent=&document_.Root;
    if(!parentPath.Indices.empty())
    {
        parentPath.Valid=true;
        parent=Resolve(parentPath);
    }
    if(!parent || removeIndex>=parent->Children.size())return false;
    parent->Children.erase(parent->Children.begin()+static_cast<std::ptrdiff_t>(removeIndex));

    if(parent!=&document_.Root)
    {
        nextSelection=parentPath;
        nextSelection.Valid=true;
    }
    else if(!document_.Root.Children.empty())
    {
        const std::size_t next=std::min(removeIndex,document_.Root.Children.size()-1u);
        nextSelection={true,{next}};
    }
    return true;
}

bool UIDesignerModel::Move(const UIDesignerPath& path,int direction,UIDesignerPath& movedPath)
{
    movedPath=path;
    if(!path.Valid || path.Indices.empty() || (direction!=1 && direction!=-1))return false;
    UIDesignerPath parentPath=path;
    const std::size_t index=parentPath.Indices.back();
    parentPath.Indices.pop_back();
    UIElementDefinition* parent=&document_.Root;
    if(!parentPath.Indices.empty())
    {
        parentPath.Valid=true;
        parent=Resolve(parentPath);
    }
    if(!parent || index>=parent->Children.size())return false;
    const long long target=static_cast<long long>(index)+direction;
    if(target<0 || target>=static_cast<long long>(parent->Children.size()))return false;
    std::swap(parent->Children[index],parent->Children[static_cast<std::size_t>(target)]);
    movedPath.Indices.back()=static_cast<std::size_t>(target);
    return true;
}

void UIDesignerModel::AppendHierarchy(const UIElementDefinition& element,
                                      const UIDesignerPath& path,int depth,
                                      std::vector<UIDesignerHierarchyItem>& out)const
{
    std::string display=TypeName(element.Type);
    const auto id=Attribute(element,"id");
    const auto text=Attribute(element,element.Type==UIElementType::Panel?"title":"text");
    if(!text.empty())display+=" : "+text;
    if(!id.empty())display+="  ["+id+"]";
    out.push_back({path,depth,std::move(display)});
    for(std::size_t i=0;i<element.Children.size();++i)
    {
        UIDesignerPath child=path;
        child.Valid=true;
        child.Indices.push_back(i);
        AppendHierarchy(element.Children[i],child,depth+1,out);
    }
}

std::vector<UIDesignerHierarchyItem> UIDesignerModel::BuildHierarchy()const
{
    std::vector<UIDesignerHierarchyItem> result;
    for(std::size_t i=0;i<document_.Root.Children.size();++i)
        AppendHierarchy(document_.Root.Children[i],{true,{i}},0,result);
    return result;
}
