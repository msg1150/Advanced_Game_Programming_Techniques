// ============================================================================
// Input.h
// ----------------------------------------------------------------------------
// Win32 Keyboard / Mouse 메시지를 프레임 단위 상태로 변환한다.
//
// Camera가 직접 GetAsyncKeyState나 Win32 메시지를 읽지 않고
// 모든 입력을 이 클래스를 통해 사용하도록 만드는 것이 목적이다.
//
// 현재 지원:
// - Keyboard Down / Pressed / Released
// - Left Mouse Button
// - Mouse Delta
// - Mouse Wheel
// ============================================================================

#pragma once

#include <Windows.h>

#include <array>

class Input
{
public:
    // 프레임이 시작될 때 Current State를 Previous State로 저장하고
    // Mouse Delta / Wheel Delta를 초기화한다.
    void BeginFrame();

    // Window에서 받은 Win32 메시지를 입력 상태로 변환한다.
    void ProcessMessage(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    // 현재 프레임에 키가 눌려 있는지.
    bool IsKeyDown(int virtualKey) const;

    // 이전 프레임에는 Up, 현재 프레임에는 Down인지.
    bool IsKeyPressed(int virtualKey) const;

    // 이전 프레임에는 Down, 현재 프레임에는 Up인지.
    bool IsKeyReleased(int virtualKey) const;

    bool IsLeftMouseDown() const;
    bool IsLeftMousePressed() const;
    bool IsLeftMouseReleased() const;

    // 이전 Mouse Position 대비 이번 프레임의 이동량.
    int GetMouseDeltaX() const;
    int GetMouseDeltaY() const;

    // Wheel 한 칸은 +1 / -1 단위로 반환한다.
    float GetMouseWheelDelta() const;

private:
    // Focus를 잃었을 때 키가 계속 눌린 상태로 남지 않도록 초기화한다.
    void ClearKeyboard();

private:
    // Virtual-Key Code 0~255 상태.
    std::array<bool, 256> currentKeys_ = {};
    std::array<bool, 256> previousKeys_ = {};

    bool currentLeftMouse_ = false;
    bool previousLeftMouse_ = false;

    // 첫 WM_MOUSEMOVE에서는 이전 위치가 없으므로
    // 잘못된 큰 Delta가 생기지 않도록 여부를 저장한다.
    bool hasMousePosition_ = false;

    int mouseX_ = 0;
    int mouseY_ = 0;

    int mouseDeltaX_ = 0;
    int mouseDeltaY_ = 0;

    float mouseWheelDelta_ = 0.0f;
};
