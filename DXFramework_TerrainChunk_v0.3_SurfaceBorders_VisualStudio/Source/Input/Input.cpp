// ============================================================================
// Input.cpp
// ============================================================================

#include "Input/Input.h"

#include <Windowsx.h>

void Input::BeginFrame()
{
    // 현재 상태를 이전 프레임 상태로 보존한다.
    previousKeys_ = currentKeys_;
    previousLeftMouse_ = currentLeftMouse_;

    // Mouse Delta와 Wheel은 "프레임 이벤트 값"이므로 매 프레임 초기화한다.
    mouseDeltaX_ = 0;
    mouseDeltaY_ = 0;
    mouseWheelDelta_ = 0.0f;
}

void Input::ProcessMessage(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        // Virtual-Key Code가 배열 범위 안일 때만 상태를 기록한다.
        if (wParam < currentKeys_.size())
        {
            currentKeys_[wParam] = true;
        }
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wParam < currentKeys_.size())
        {
            currentKeys_[wParam] = false;
        }
        break;

    case WM_KILLFOCUS:
        // Alt+Tab 등으로 Window Focus를 잃었는데
        // KeyUp 메시지를 받지 못하는 상황을 방어한다.
        ClearKeyboard();
        currentLeftMouse_ = false;
        hasMousePosition_ = false;
        break;

    case WM_LBUTTONDOWN:
        currentLeftMouse_ = true;

        // Drag 도중 Cursor가 Window 밖으로 나가더라도
        // MouseUp을 받을 수 있도록 Mouse Capture를 시작한다.
        SetCapture(hwnd);
        break;

    case WM_LBUTTONUP:
        currentLeftMouse_ = false;
        ReleaseCapture();
        break;

    case WM_MOUSEMOVE:
    {
        const int newX =
            GET_X_LPARAM(lParam);

        const int newY =
            GET_Y_LPARAM(lParam);

        // 이전 위치가 존재할 때만 Delta를 계산한다.
        if (hasMousePosition_)
        {
            mouseDeltaX_ +=
                newX - mouseX_;

            mouseDeltaY_ +=
                newY - mouseY_;
        }

        mouseX_ = newX;
        mouseY_ = newY;
        hasMousePosition_ = true;
        break;
    }

    case WM_MOUSEWHEEL:
        // Windows 기본 Wheel Delta(120)를
        // 사용하기 편한 1.0 단위로 정규화한다.
        mouseWheelDelta_ +=
            static_cast<float>(
                GET_WHEEL_DELTA_WPARAM(wParam)) /
            static_cast<float>(WHEEL_DELTA);
        break;
    }
}

bool Input::IsKeyDown(int virtualKey) const
{
    if (virtualKey < 0 ||
        virtualKey >= 256)
    {
        return false;
    }

    return currentKeys_[virtualKey];
}

bool Input::IsKeyPressed(int virtualKey) const
{
    if (virtualKey < 0 ||
        virtualKey >= 256)
    {
        return false;
    }

    return
        currentKeys_[virtualKey] &&
        !previousKeys_[virtualKey];
}

bool Input::IsKeyReleased(int virtualKey) const
{
    if (virtualKey < 0 ||
        virtualKey >= 256)
    {
        return false;
    }

    return
        !currentKeys_[virtualKey] &&
        previousKeys_[virtualKey];
}

bool Input::IsLeftMouseDown() const
{
    return currentLeftMouse_;
}

bool Input::IsLeftMousePressed() const
{
    return
        currentLeftMouse_ &&
        !previousLeftMouse_;
}

bool Input::IsLeftMouseReleased() const
{
    return
        !currentLeftMouse_ &&
        previousLeftMouse_;
}

int Input::GetMouseDeltaX() const
{
    return mouseDeltaX_;
}

int Input::GetMouseDeltaY() const
{
    return mouseDeltaY_;
}

float Input::GetMouseWheelDelta() const
{
    return mouseWheelDelta_;
}

void Input::ClearKeyboard()
{
    currentKeys_.fill(false);
}
