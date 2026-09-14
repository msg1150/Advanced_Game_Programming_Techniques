// ============================================================================
// ShowcaseVisibilityController.h
// ----------------------------------------------------------------------------
// 현재 샘플 화면에 배치된 테스트 오브젝트/기능의 표시 상태를 관리하는
// 공용 Utility 클래스.
//
// 설계 목표
// 1. 기존 Terrain / Renderer / Shader 코드를 수정하지 않는다.
// 2. Application이 이 클래스를 통해 "보일지/숨길지"만 제어한다.
// 3. 숫자키 1~0을 확장 가능한 고정 슬롯으로 사용한다.
// 4. 나중에 Triplanar Terrain 같은 새 기능이 추가되어도
//    새 슬롯 Label만 등록하면 같은 방식으로 바로 토글할 수 있다.
// 5. 제거가 필요하면 이 파일 2개와 Application 연결부만 제거하면 된다.
// ============================================================================

#pragma once

#include "Input/Input.h"

#include <array>
#include <cstddef>
#include <string>

constexpr std::size_t kShowcaseVisibilitySlotCount = 10u;

struct ShowcaseVisibilitySlot
{
    std::wstring Label;
    bool Assigned = false;
    bool Visible = false;
};

class ShowcaseVisibilityController
{
public:
    // 특정 슬롯에 이름과 기본 표시 상태를 등록한다.
    //
    // 슬롯 번호와 실제 키의 대응:
    // 0 -> [1]
    // 1 -> [2]
    // 2 -> [3]
    // 3 -> [4]
    // 4 -> [5]
    // 5 -> [6]
    // 6 -> [7]
    // 7 -> [8]
    // 8 -> [9]
    // 9 -> [0]
    void SetSlot(
        std::size_t slotIndex,
        const std::wstring& label,
        bool visibleByDefault);

    // 매 프레임 Input을 확인해 숫자키 토글을 처리한다.
    void Update(
        const Input& input);

    // 현재 슬롯이 "배정되어 있고 보이는 상태"인지 반환한다.
    bool IsVisible(
        std::size_t slotIndex) const;

    // 화면 Debug Overlay에 바로 출력할 수 있는 상태 문자열을 만든다.
    std::wstring BuildDebugText() const;

private:
    void ToggleSlot(
        std::size_t slotIndex);

    bool IsSlotIndexValid(
        std::size_t slotIndex) const;

    static bool TryConvertVirtualKeyToSlot(
        int virtualKey,
        std::size_t& outSlotIndex);

    static std::wstring GetKeyLabelForSlot(
        std::size_t slotIndex);

private:
    std::array<
        ShowcaseVisibilitySlot,
        kShowcaseVisibilitySlotCount>
        slots_ = {};
};
