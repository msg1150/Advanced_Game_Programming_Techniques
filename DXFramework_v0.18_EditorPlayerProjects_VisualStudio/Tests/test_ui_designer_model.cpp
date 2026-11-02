#include "Tools/UIDesigner/Core/UIDesignerModel.h"
#include "UI/Document/Serialization/UIXmlParser.h"
#include <cassert>
#include <filesystem>
#include <string>
#include <vector>
int main()
{
    UIDesignerModel model;
    auto panel=model.FirstPanel();assert(panel.Valid);
    auto label=model.AddElement(UIElementType::Label,panel);assert(label.Valid);
    auto* labelDef=model.Resolve(label);assert(labelDef&&labelDef->Type==UIElementType::Label);
    UIDesignerModel::SetAttribute(*labelDef,"text","Designer Test");
    auto slider=model.AddElement(UIElementType::Slider,label);assert(slider.Valid);
    auto column=model.FindAncestor(slider,UIElementType::Column);assert(column.Valid);
    auto second=model.AddElement(UIElementType::Button,column);assert(second.Valid);
    UIDesignerPath moved;assert(model.Move(second,-1,moved));
    auto rows=model.BuildHierarchy();assert(rows.size()>=5u);
    const auto path=std::filesystem::temp_directory_path()/"dxframework_ui_designer_model_test.ui.xml";
    assert(model.Save(path));
    UIDesignerModel loaded;std::vector<UIDiagnostic> diagnostics;
    assert(loaded.Load(path,diagnostics));assert(diagnostics.empty());
    assert(loaded.BuildHierarchy().size()==rows.size());
    UIDesignerPath next;assert(loaded.Remove(loaded.FirstPanel(),next));
    std::error_code ec;std::filesystem::remove(path,ec);
    return 0;
}
