// UIDocument XML Parse/UTF-8/Writer Round-trip 검증.
#include "UI/Document/UIUtf8.h"
#include "UI/Document/UIXmlParser.h"
#include "UI/Document/UIXmlWriter.h"
#include <cassert>
#include <string>
int main()
{
    const std::string xml=R"XML(<?xml version="1.0" encoding="utf-8"?>
<!-- comment -->
<UIDocument version="1">
  <Panel id="P" title="한글 &amp; XML" width="320" height="240">
    <Column><Label id="L" text="테스트" /><Button text="A &lt; B" /></Column>
  </Panel>
</UIDocument>)XML";
    UIDocumentDefinition doc;std::vector<UIDiagnostic> diag;
    assert(UIXmlParser::Parse(xml,doc,diag));
    assert(doc.Root.Type==UIElementType::Document&&doc.Root.Children.size()==1u);
    const auto& panel=doc.Root.Children[0];
    assert(panel.Find("title")&&*panel.Find("title")=="한글 & XML");
    assert(UIUtf8::ToWide("한글")==L"한글");
    const std::string saved=UIXmlWriter::Write(doc);
    UIDocumentDefinition roundTrip;std::vector<UIDiagnostic> diag2;
    assert(UIXmlParser::Parse(saved,roundTrip,diag2));
    assert(roundTrip.Root.Children.size()==1u);
    UIDocumentDefinition bad;std::vector<UIDiagnostic> badDiag;
    assert(!UIXmlParser::Parse("<UIDocument><Panel></UIDocument>",bad,badDiag));
    assert(!badDiag.empty());
    return 0;
}
