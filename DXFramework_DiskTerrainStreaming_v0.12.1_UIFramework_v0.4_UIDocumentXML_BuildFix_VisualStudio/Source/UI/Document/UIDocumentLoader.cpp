#include "UI/Document/UIDocumentLoader.h"
#include "UI/Document/UIBindingRegistry.h"
#include "UI/Document/UIWidgetFactory.h"
#include "UI/Document/UIXmlParser.h"
UIDocumentLoadResult UIDocumentLoader::LoadFile(UIManager& ui,const UIBindingRegistry& bindings,
                                                const std::filesystem::path& path)
{
    UIDocumentLoadResult result;
    if(!UIXmlParser::ParseFile(path,result.Definition,result.Diagnostics))return result;
    result.Success=UIWidgetFactory::Build(ui,result.Definition,bindings,result.Instance,result.Diagnostics);
    return result;
}
UIDocumentLoadResult UIDocumentLoader::LoadString(UIManager& ui,const UIBindingRegistry& bindings,
                                                  std::string_view utf8)
{
    UIDocumentLoadResult result;
    if(!UIXmlParser::Parse(utf8,result.Definition,result.Diagnostics))return result;
    result.Success=UIWidgetFactory::Build(ui,result.Definition,bindings,result.Instance,result.Diagnostics);
    return result;
}
