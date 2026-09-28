// UITheme.h : 게임 기능과 무관한 공통 UI 스타일 및 레이아웃 수치.
#pragma once
#include "UI/UIRenderer.h"
namespace UITheme
{
    inline constexpr wchar_t FontFamily[]=L"Malgun Gothic";
    inline constexpr wchar_t FontLocale[]=L"ko-kr";
    inline constexpr float NormalFontSize=14.f;
    inline constexpr float SmallFontSize=12.f;
    inline constexpr float HeadingFontSize=19.f;
    inline constexpr UIColor Text{.91f,.95f,.98f,1.f};
    inline constexpr UIColor Muted{.65f,.73f,.80f,1.f};
    inline constexpr UIColor Accent{.18f,.79f,.61f,1.f};
    inline constexpr UIColor PanelBackground{.065f,.09f,.13f,.95f};
    inline constexpr UIColor PanelBorder{.29f,.38f,.46f,1.f};
    inline constexpr UIColor Divider{.23f,.32f,.39f,1.f};
    inline constexpr UIColor CheckOff{.20f,.27f,.34f,1.f};
    inline constexpr UIColor CheckBorder{.48f,.55f,.64f,1.f};
    inline constexpr UIColor CheckMark{.06f,.15f,.14f,1.f};
    inline constexpr UIColor SliderTrack{.24f,.32f,.39f,1.f};
    inline constexpr UIColor SliderKnob{.80f,.94f,.90f,1.f};
    inline constexpr UIColor SliderKnobHover{.85f,1.f,.93f,1.f};
    inline constexpr UIColor StatsText{.92f,.95f,.98f,1.f};
    inline constexpr UIColor StatsMuted{.65f,.75f,.83f,1.f};
    inline constexpr UIColor StatsError{1.f,.48f,.42f,1.f};
    inline constexpr UIColor RowHover{.18f,.24f,.31f,.9f};
    inline constexpr UIColor ButtonHover{.18f,.59f,.49f,.98f};
    inline constexpr UIColor ButtonNormal{.12f,.37f,.35f,.96f};
    inline constexpr UIColor InputFocused{.13f,.29f,.29f,1.f};
    inline constexpr UIColor InputNormal{.13f,.19f,.25f,1.f};
    inline constexpr UIColor InputBorder{.38f,.48f,.56f,1.f};
    // 통계창의 기존 합의값. 텍스트/테두리는 별도 불투명 색으로 렌더링.
    inline constexpr float StatsBackgroundOpacity=.68f;
    inline constexpr float PanelHeaderHeight=43.f;
    inline constexpr float PanelContentTop=51.f;
    inline constexpr float PanelMargin=12.f;
    inline constexpr float ItemSpacing=2.f;
    inline constexpr float ColumnGap=12.f;
}
