// ============================================================================
// CollisionEditorPanel.h
// ----------------------------------------------------------------------------
// 기존 Custom UI Framework의 Panel/Label/Button/CheckBox/Slider만 사용해서
// Collider 생성/선택/수정/삭제를 제공한다. Physics Core는 UI를 include하지 않는다.
// ============================================================================
#pragma once

class CollisionScene;
class UIManager;

class CollisionEditorPanel
{
public:
    static void Register(UIManager& ui,CollisionScene& scene);
};
