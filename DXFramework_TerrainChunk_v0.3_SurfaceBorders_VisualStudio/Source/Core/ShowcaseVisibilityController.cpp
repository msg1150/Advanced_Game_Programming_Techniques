// ============================================================================
// ShowcaseVisibilityController.cpp
// ============================================================================

#include "Core/ShowcaseVisibilityController.h"

#include <sstream>

void ShowcaseVisibilityController::SetSlot(
    std::size_t slotIndex,
    const std::wstring& label,
    bool visibleByDefault)
{
    if (!IsSlotIndexValid(slotIndex))
    {
        return;
    }

    slots_[slotIndex].Assigned =
        true;

    slots_[slotIndex].Label =
        label;

    slots_[slotIndex].Visible =
        visibleByDefault;
}

void ShowcaseVisibilityController::Update(
    const Input& input)
{
    // 숫자키 1~9, 0을 고정 토글 키로 사용한다.
    constexpr int kToggleKeys[kShowcaseVisibilitySlotCount] =
    {
        '1', '2', '3', '4', '5',
        '6', '7', '8', '9', '0'
    };

    for (const int key : kToggleKeys)
    {
        if (!input.IsKeyPressed(key))
        {
            continue;
        }

        std::size_t slotIndex = 0u;

        if (TryConvertVirtualKeyToSlot(
                key,
                slotIndex))
        {
            ToggleSlot(
                slotIndex);
        }
    }
}

bool ShowcaseVisibilityController::IsVisible(
    std::size_t slotIndex) const
{
    if (!IsSlotIndexValid(slotIndex))
    {
        return false;
    }

    const ShowcaseVisibilitySlot& slot =
        slots_[slotIndex];

    return slot.Assigned &&
           slot.Visible;
}

std::wstring ShowcaseVisibilityController::BuildDebugText() const
{
    std::wstringstream stream;

    // 한 줄에 2~3개씩 나눠서 출력해 가독성을 높인다.
    // 현재는 최대 10 슬롯이므로 4줄 이내로 표현된다.
    std::size_t printedCount = 0u;

    for (std::size_t slotIndex = 0u;
         slotIndex < slots_.size();
         ++slotIndex)
    {
        const ShowcaseVisibilitySlot& slot =
            slots_[slotIndex];

        stream
            << L"["
            << GetKeyLabelForSlot(slotIndex)
            << L"] ";

        if (slot.Assigned)
        {
            stream
                << slot.Label
                << L" : "
                << (
                    slot.Visible
                    ? L"ON"
                    : L"OFF");
        }
        else
        {
            stream
                << L"Reserved";
        }

        ++printedCount;

        if (slotIndex + 1u < slots_.size())
        {
            // 보기 좋게 적당한 위치에서 줄바꿈한다.
            if (printedCount == 3u ||
                printedCount == 6u ||
                printedCount == 8u)
            {
                stream
                    << L"\n";
            }
            else
            {
                stream
                    << L"    ";
            }
        }
    }

    return stream.str();
}

void ShowcaseVisibilityController::ToggleSlot(
    std::size_t slotIndex)
{
    if (!IsSlotIndexValid(slotIndex))
    {
        return;
    }

    ShowcaseVisibilitySlot& slot =
        slots_[slotIndex];

    // 아직 어떤 기능도 배정되지 않은 예약 슬롯은 토글하지 않는다.
    if (!slot.Assigned)
    {
        return;
    }

    slot.Visible =
        !slot.Visible;
}

bool ShowcaseVisibilityController::IsSlotIndexValid(
    std::size_t slotIndex) const
{
    return slotIndex <
           slots_.size();
}

bool ShowcaseVisibilityController::TryConvertVirtualKeyToSlot(
    int virtualKey,
    std::size_t& outSlotIndex)
{
    switch (virtualKey)
    {
    case '1': outSlotIndex = 0u; return true;
    case '2': outSlotIndex = 1u; return true;
    case '3': outSlotIndex = 2u; return true;
    case '4': outSlotIndex = 3u; return true;
    case '5': outSlotIndex = 4u; return true;
    case '6': outSlotIndex = 5u; return true;
    case '7': outSlotIndex = 6u; return true;
    case '8': outSlotIndex = 7u; return true;
    case '9': outSlotIndex = 8u; return true;
    case '0': outSlotIndex = 9u; return true;
    default:
        return false;
    }
}

std::wstring ShowcaseVisibilityController::GetKeyLabelForSlot(
    std::size_t slotIndex)
{
    switch (slotIndex)
    {
    case 0u: return L"1";
    case 1u: return L"2";
    case 2u: return L"3";
    case 3u: return L"4";
    case 4u: return L"5";
    case 5u: return L"6";
    case 6u: return L"7";
    case 7u: return L"8";
    case 8u: return L"9";
    case 9u: return L"0";
    default:
        return L"?";
    }
}
