// ============================================================================
// UIXmlParser.cpp : 외부 라이브러리 없이 UI 전용 XML을 읽는 작은 Parser.
// ============================================================================
#include "UI/Document/UIXmlParser.h"
#include <charconv>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
namespace
{
UIElementType ToType(std::string_view tag)
{
    if(tag=="UIDocument")return UIElementType::Document;
    if(tag=="Panel")return UIElementType::Panel;
    if(tag=="Column")return UIElementType::Column;
    if(tag=="Label")return UIElementType::Label;
    if(tag=="CheckBox")return UIElementType::CheckBox;
    if(tag=="Slider")return UIElementType::Slider;
    if(tag=="Button")return UIElementType::Button;
    return UIElementType::Unknown;
}
std::string DecodeEntities(std::string_view value)
{
    std::string out;out.reserve(value.size());
    for(std::size_t i=0;i<value.size();++i)
    {
        if(value[i]!='&'){out.push_back(value[i]);continue;}
        const auto end=value.find(';',i+1u);
        if(end==std::string_view::npos){out.push_back('&');continue;}
        const auto entity=value.substr(i+1u,end-i-1u);
        if(entity=="amp")out.push_back('&');
        else if(entity=="lt")out.push_back('<');
        else if(entity=="gt")out.push_back('>');
        else if(entity=="quot")out.push_back('"');
        else if(entity=="apos")out.push_back('\'');
        else if(!entity.empty()&&entity[0]=='#')
        {
            unsigned int cp=0;bool ok=false;
            if(entity.size()>2u&&(entity[1]=='x'||entity[1]=='X'))
            {
                const auto result=std::from_chars(entity.data()+2,entity.data()+entity.size(),cp,16);
                ok=result.ec==std::errc{}&&result.ptr==entity.data()+entity.size();
            }
            else
            {
                const auto result=std::from_chars(entity.data()+1,entity.data()+entity.size(),cp,10);
                ok=result.ec==std::errc{}&&result.ptr==entity.data()+entity.size();
            }
            if(ok&&cp<=0x7Fu)out.push_back(static_cast<char>(cp));
            else
            {
                // 비 ASCII Numeric Entity는 UTF-8로 다시 작성한다.
                if(ok&&cp<=0x7FFu){out.push_back(static_cast<char>(0xC0u|(cp>>6u)));out.push_back(static_cast<char>(0x80u|(cp&0x3Fu)));}
                else if(ok&&cp<=0xFFFFu){out.push_back(static_cast<char>(0xE0u|(cp>>12u)));out.push_back(static_cast<char>(0x80u|((cp>>6u)&0x3Fu)));out.push_back(static_cast<char>(0x80u|(cp&0x3Fu)));}
                else if(ok&&cp<=0x10FFFFu){out.push_back(static_cast<char>(0xF0u|(cp>>18u)));out.push_back(static_cast<char>(0x80u|((cp>>12u)&0x3Fu)));out.push_back(static_cast<char>(0x80u|((cp>>6u)&0x3Fu)));out.push_back(static_cast<char>(0x80u|(cp&0x3Fu)));}
                else{out.append(value.substr(i,end-i+1u));}
            }
        }
        else{out.append(value.substr(i,end-i+1u));}
        i=end;
    }
    return out;
}
class Parser
{
public:
    Parser(std::string_view source,std::vector<UIDiagnostic>& diagnostics)
        :s_(source),diagnostics_(diagnostics)
    {
        if(s_.size()>=3u&&static_cast<unsigned char>(s_[0])==0xEFu&&
           static_cast<unsigned char>(s_[1])==0xBBu&&static_cast<unsigned char>(s_[2])==0xBFu)
            Advance(3u);
    }
    bool Run(UIDocumentDefinition& out)
    {
        SkipMisc();
        if(End()){Error("UIDocument root element가 없습니다.");return false;}
        UIElementDefinition root;
        if(!Element(root))return false;
        SkipMisc();
        if(!End()){Error("Root element 뒤에 추가 내용이 있습니다.");return false;}
        if(root.Type!=UIElementType::Document)
        {ErrorAt(root.Line,root.Column,"Root element는 <UIDocument>여야 합니다.");return false;}
        out.Root=std::move(root);
        if(const auto* version=out.Root.Find("version"))
        {
            unsigned int value=0;
            const auto result=std::from_chars(version->data(),version->data()+version->size(),value);
            if(result.ec!=std::errc{}||result.ptr!=version->data()+version->size()||value==0u)
            {ErrorAt(out.Root.Line,out.Root.Column,"UIDocument version은 1 이상의 정수여야 합니다.");return false;}
            out.Version=value;
        }
        return true;
    }
private:
    bool End()const{return pos_>=s_.size();}
    char Peek(std::size_t offset=0u)const{return pos_+offset<s_.size()?s_[pos_+offset]:'\0';}
    bool Starts(std::string_view text)const{return s_.substr(pos_,text.size())==text;}
    void Advance(std::size_t count=1u)
    {
        for(std::size_t n=0;n<count&&pos_<s_.size();++n)
        {if(s_[pos_]=='\n'){++line_;column_=1u;}else ++column_;++pos_;}
    }
    void Space(){while(!End()&&std::isspace(static_cast<unsigned char>(Peek())))Advance();}
    void SkipMisc()
    {
        while(true)
        {
            Space();
            if(Starts("<?"))
            {
                const auto end=s_.find("?>",pos_+2u);
                if(end==std::string_view::npos){Error("닫히지 않은 XML 선언입니다.");pos_=s_.size();return;}
                Advance(end+2u-pos_);continue;
            }
            if(Starts("<!--"))
            {
                const auto end=s_.find("-->",pos_+4u);
                if(end==std::string_view::npos){Error("닫히지 않은 XML Comment입니다.");pos_=s_.size();return;}
                Advance(end+3u-pos_);continue;
            }
            break;
        }
    }
    static bool NameStart(char c){return std::isalpha(static_cast<unsigned char>(c))||c=='_'||c==':';}
    static bool NameChar(char c){return NameStart(c)||std::isdigit(static_cast<unsigned char>(c))||c=='-'||c=='.';}
    bool Name(std::string& out)
    {
        if(!NameStart(Peek()))return false;
        const auto begin=pos_;Advance();
        while(NameChar(Peek()))Advance();
        out=std::string(s_.substr(begin,pos_-begin));return true;
    }
    bool Quoted(std::string& out)
    {
        const char quote=Peek();if(quote!='\''&&quote!='"')return false;Advance();
        const auto begin=pos_;
        while(!End()&&Peek()!=quote)Advance();
        if(End()){Error("닫히지 않은 Attribute 문자열입니다.");return false;}
        out=DecodeEntities(s_.substr(begin,pos_-begin));Advance();return true;
    }
    bool Element(UIElementDefinition& out)
    {
        SkipMisc();
        if(Peek()!='<'){Error("Element 시작 '<'가 필요합니다.");return false;}
        const auto startLine=line_,startColumn=column_;Advance();
        if(Peek()=='/'){Error("예상하지 못한 닫는 Tag입니다.");return false;}
        std::string tag;if(!Name(tag)){Error("Tag 이름이 필요합니다.");return false;}
        out.Tag=tag;out.Type=ToType(tag);out.Line=startLine;out.Column=startColumn;
        while(true)
        {
            Space();
            if(Starts("/>")){Advance(2u);return true;}
            if(Peek()=='>'){Advance();break;}
            std::string key,value;
            if(!Name(key)){Error("Attribute 이름이 필요합니다.");return false;}
            Space();if(Peek()!='='){Error("Attribute 뒤에 '='가 필요합니다.");return false;}Advance();Space();
            if(!Quoted(value))return false;
            for(const auto& existing:out.Attributes)
                if(existing.Name==key){Error("중복 Attribute: "+key);return false;}
            out.Attributes.push_back({std::move(key),std::move(value)});
        }
        while(true)
        {
            SkipMisc();
            if(Starts("</"))
            {
                Advance(2u);std::string close;if(!Name(close)){Error("닫는 Tag 이름이 필요합니다.");return false;}
                Space();if(Peek()!='>'){Error("닫는 Tag 뒤에 '>'가 필요합니다.");return false;}Advance();
                if(close!=tag){Error("Tag가 일치하지 않습니다. <"+tag+"> / </"+close+">");return false;}
                return true;
            }
            if(End()){Error("닫히지 않은 Tag: "+tag);return false;}
            if(Peek()=='<')
            {
                UIElementDefinition child;if(!Element(child))return false;
                out.Children.push_back(std::move(child));continue;
            }
            // 현재 문서 포맷은 Text Node 대신 text="..." Attribute만 사용합니다.
            const auto begin=pos_;while(!End()&&Peek()!='<')Advance();
            for(const char c:s_.substr(begin,pos_-begin))
                if(!std::isspace(static_cast<unsigned char>(c)))
                {ErrorAt(startLine,startColumn,"Text Node는 지원하지 않습니다. text Attribute를 사용하세요.");return false;}
        }
    }
    void Error(std::string message){ErrorAt(line_,column_,std::move(message));}
    void ErrorAt(std::size_t line,std::size_t column,std::string message)
    {diagnostics_.push_back({UIDiagnosticSeverity::Error,line,column,std::move(message)});}
    std::string_view s_;std::vector<UIDiagnostic>& diagnostics_;
    std::size_t pos_=0u,line_=1u,column_=1u;
};
}
bool UIXmlParser::Parse(std::string_view utf8,UIDocumentDefinition& out,
                        std::vector<UIDiagnostic>& diagnostics)
{
    diagnostics.clear();out=UIDocumentDefinition{};
    Parser parser(utf8,diagnostics);return parser.Run(out);
}
bool UIXmlParser::ParseFile(const std::filesystem::path& path,UIDocumentDefinition& out,
                            std::vector<UIDiagnostic>& diagnostics)
{
    std::ifstream file(path,std::ios::binary);
    if(!file)
    {
        diagnostics.clear();
        diagnostics.push_back({UIDiagnosticSeverity::Error,1u,1u,
            "UI 문서를 열 수 없습니다: "+path.string()});
        return false;
    }
    const std::string data((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    return Parse(data,out,diagnostics);
}
