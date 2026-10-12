// ============================================================================
// CollisionEditorPanel.cpp
// ============================================================================
#include "Features/CollisionEditor/CollisionEditorPanel.h"
#include "Features/CollisionEditor/CollisionScene.h"
#include "UI/UIManager.h"

#include <memory>

void CollisionEditorPanel::Register(UIManager& ui,CollisionScene& scene)
{
    const auto panel=ui.CreatePanel(
        L"Collision / GPU Picking",
        L"Collision 열기",
        L"Collision 닫기",
        540.f,650.f);

    // ---------------------------------------------------------------------
    // 1열: Scene / 생성 / Debug 제어
    // ---------------------------------------------------------------------
    ui.AddToPanel(panel,std::make_unique<UILabel>([&scene]{return scene.SceneStatusText();}));
    ui.AddToPanel(panel,std::make_unique<UILabel>([&scene]{return scene.PickStatusText();}));
    ui.AddToPanel(panel,std::make_unique<UICheckBox>(L"GPU Picking",
        [&scene]{return scene.IsGPUPickingEnabled();},
        [&scene](bool v){scene.SetGPUPickingEnabled(v);}));
    ui.AddToPanel(panel,std::make_unique<UICheckBox>(L"Collider Debug Wireframe",
        [&scene]{return scene.IsDebugVisible();},
        [&scene](bool v){scene.SetDebugVisible(v);}));
    ui.AddToPanel(panel,std::make_unique<UICheckBox>(L"충돌 중 Collider 강조",
        [&scene]{return scene.IsCollisionHighlightEnabled();},
        [&scene](bool v){scene.SetCollisionHighlightEnabled(v);}));
    ui.AddToPanel(panel,std::make_unique<UICheckBox>(L"Contact Point / Normal 표시",
        [&scene]{return scene.IsContactDebugEnabled();},
        [&scene](bool v){scene.SetContactDebugEnabled(v);}));

    ui.AddToPanel(panel,std::make_unique<UILabel>(L"새 Collider (Camera Target 주변 생성)"));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"Sphere 추가",[&scene]{scene.AddSphere();}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"AABB 추가",[&scene]{scene.AddAABB();}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"OBB 추가",[&scene]{scene.AddOBB();}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"Capsule 추가",[&scene]{scene.AddCapsule();}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"선택 해제",[&scene]{scene.ClearSelection();}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"선택 삭제",[&scene]{scene.DeleteSelected();}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"전체 Collider 삭제",[&scene]{scene.ClearAll();}));
    ui.AddToPanel(panel,std::make_unique<UILabel>(L"3D Scene의 Wireframe 내부를 클릭하면 GPU ID로 선택합니다."));

    // ---------------------------------------------------------------------
    // 2열: 선택 Collider Inspector
    // ---------------------------------------------------------------------
    ui.StartSecondColumn(panel);
    ui.AddToPanel(panel,std::make_unique<UILabel>([&scene]{return scene.SelectionText();}));
    ui.AddToPanel(panel,std::make_unique<UILabel>([&scene]{return scene.ShapeParameterHint();}));
    ui.AddToPanel(panel,std::make_unique<UICheckBox>(L"Enabled",
        [&scene]{return scene.SelectedEnabled();},
        [&scene](bool v){scene.SetSelectedEnabled(v);}));
    ui.AddToPanel(panel,std::make_unique<UICheckBox>(L"Trigger (감지만 / Response 없음)",
        [&scene]{return scene.SelectedTrigger();},
        [&scene](bool v){scene.SetSelectedTrigger(v);}));

    ui.AddToPanel(panel,std::make_unique<UILabel>(L"Position"));
    ui.AddToPanel(panel,std::make_unique<UISlider>(L"Position X",-256.f,256.f,1.f,
        [&scene]{return scene.GetPositionX();},[&scene](float v){scene.SetPositionX(v);}));
    ui.AddToPanel(panel,std::make_unique<UISlider>(L"Position Y",-32.f,96.f,1.f,
        [&scene]{return scene.GetPositionY();},[&scene](float v){scene.SetPositionY(v);}));
    ui.AddToPanel(panel,std::make_unique<UISlider>(L"Position Z",-256.f,256.f,1.f,
        [&scene]{return scene.GetPositionZ();},[&scene](float v){scene.SetPositionZ(v);}));

    ui.AddToPanel(panel,std::make_unique<UILabel>(L"Rotation (Degree)"));
    ui.AddToPanel(panel,std::make_unique<UISlider>(L"Rotation X",-180.f,180.f,5.f,
        [&scene]{return scene.GetRotationXDegrees();},[&scene](float v){scene.SetRotationXDegrees(v);}));
    ui.AddToPanel(panel,std::make_unique<UISlider>(L"Rotation Y",-180.f,180.f,5.f,
        [&scene]{return scene.GetRotationYDegrees();},[&scene](float v){scene.SetRotationYDegrees(v);}));
    ui.AddToPanel(panel,std::make_unique<UISlider>(L"Rotation Z",-180.f,180.f,5.f,
        [&scene]{return scene.GetRotationZDegrees();},[&scene](float v){scene.SetRotationZDegrees(v);}));

    ui.AddToPanel(panel,std::make_unique<UILabel>(L"Sphere / Capsule"));
    ui.AddToPanel(panel,std::make_unique<UISlider>(L"Radius",1.f,12.f,1.f,
        [&scene]{return scene.GetRadius();},[&scene](float v){scene.SetRadius(v);}));
    ui.AddToPanel(panel,std::make_unique<UISlider>(L"Capsule Half Segment",0.f,12.f,1.f,
        [&scene]{return scene.GetHalfSegment();},[&scene](float v){scene.SetHalfSegment(v);}));

    ui.AddToPanel(panel,std::make_unique<UILabel>(L"AABB / OBB Half Extent"));
    ui.AddToPanel(panel,std::make_unique<UISlider>(L"Half Extent X",1.f,16.f,1.f,
        [&scene]{return scene.GetHalfExtentX();},[&scene](float v){scene.SetHalfExtentX(v);}));
    ui.AddToPanel(panel,std::make_unique<UISlider>(L"Half Extent Y",1.f,16.f,1.f,
        [&scene]{return scene.GetHalfExtentY();},[&scene](float v){scene.SetHalfExtentY(v);}));
    ui.AddToPanel(panel,std::make_unique<UISlider>(L"Half Extent Z",1.f,16.f,1.f,
        [&scene]{return scene.GetHalfExtentZ();},[&scene](float v){scene.SetHalfExtentZ(v);}));
}
