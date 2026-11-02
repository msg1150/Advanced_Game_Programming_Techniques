// ============================================================================
// UIDesignerModel.h
// ----------------------------------------------------------------------------
// UI Designer가 편집하는 UIDocumentDefinition을 Win32와 분리해 관리합니다.
// Runtime Loader와 같은 IR/XML Parser/Writer를 사용하므로 Designer 전용 포맷을
// 따로 만들지 않습니다. 이 파일은 Windows API에 의존하지 않아 단독 테스트가 가능합니다.
// ============================================================================
#pragma once
#include "UI/Document/Model/UIDocument.h"
#include <filesystem>
#include <string>
#include <vector>

struct UIDesignerPath
{
    bool Valid=false;
    std::vector<std::size_t> Indices;

    bool operator==(const UIDesignerPath& other)const noexcept
    {return Valid==other.Valid && Indices==other.Indices;}
};

struct UIDesignerHierarchyItem
{
    UIDesignerPath Path;
    int Depth=0;
    std::string DisplayUtf8;
};

class UIDesignerModel
{
public:
    UIDesignerModel();

    void NewDocument();
    bool Load(const std::filesystem::path& path,std::vector<UIDiagnostic>& diagnostics);
    bool Save(const std::filesystem::path& path)const;

    const UIDocumentDefinition& Document()const{return document_;}
    UIDocumentDefinition& Document(){return document_;}
    const std::filesystem::path& CurrentPath()const{return currentPath_;}
    void SetCurrentPath(std::filesystem::path path){currentPath_=std::move(path);}

    const UIElementDefinition* Resolve(const UIDesignerPath& path)const;
    UIElementDefinition* Resolve(const UIDesignerPath& path);
    UIDesignerPath ParentOf(const UIDesignerPath& path)const;
    UIDesignerPath FindAncestor(const UIDesignerPath& path,UIElementType type)const;
    UIDesignerPath FirstPanel()const;

    std::vector<UIDesignerHierarchyItem> BuildHierarchy()const;
    UIDesignerPath AddElement(UIElementType type,const UIDesignerPath& context);
    bool Remove(const UIDesignerPath& path,UIDesignerPath& nextSelection);
    bool Move(const UIDesignerPath& path,int direction,UIDesignerPath& movedPath);

    static std::string Attribute(const UIElementDefinition& element,const char* name,
                                 std::string fallback={});
    static void SetAttribute(UIElementDefinition& element,std::string name,std::string value,
                             bool removeWhenEmpty=false);
    static void RemoveAttribute(UIElementDefinition& element,const char* name);
    static const char* TypeName(UIElementType type)noexcept;

private:
    UIDesignerPath AddPanel();
    UIDesignerPath AddColumn(const UIDesignerPath& panelPath);
    UIDesignerPath TargetPanel(const UIDesignerPath& context)const;
    UIDesignerPath TargetColumn(const UIDesignerPath& context);
    std::string UniqueId(const char* prefix)const;
    bool IdExists(const UIElementDefinition& element,const std::string& id)const;
    void AppendHierarchy(const UIElementDefinition& element,const UIDesignerPath& path,int depth,
                         std::vector<UIDesignerHierarchyItem>& out)const;

    UIDocumentDefinition document_;
    std::filesystem::path currentPath_;
};
