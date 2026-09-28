#include "UI/Document/UIXmlWriter.h"
#include <fstream>
#include <string_view>
namespace
{
std::string Escape(std::string_view value)
{
    std::string out;
    for(const char c:value)
    {
        switch(c)
        {
        case '&':out+="&amp;";break;case '<':out+="&lt;";break;case '>':out+="&gt;";break;
        case '"':out+="&quot;";break;case '\'':out+="&apos;";break;default:out.push_back(c);break;
        }
    }
    return out;
}
void Element(std::string& out,const UIElementDefinition& element,int depth)
{
    out.append(static_cast<std::size_t>(depth)*2u,' ');out+='<';out+=element.Tag;
    for(const auto& attribute:element.Attributes)
    {out+=' ';out+=attribute.Name;out+="=\"";out+=Escape(attribute.Value);out+='"';}
    if(element.Children.empty()){out+=" />\n";return;}
    out+=">\n";
    for(const auto& child:element.Children)Element(out,child,depth+1);
    out.append(static_cast<std::size_t>(depth)*2u,' ');out+="</";out+=element.Tag;out+=">\n";
}
}
std::string UIXmlWriter::Write(const UIDocumentDefinition& document)
{
    std::string out="<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    Element(out,document.Root,0);return out;
}
bool UIXmlWriter::WriteFile(const std::filesystem::path& path,const UIDocumentDefinition& document)
{
    std::ofstream file(path,std::ios::binary|std::ios::trunc);if(!file)return false;
    const auto text=Write(document);file.write(text.data(),static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(file);
}
