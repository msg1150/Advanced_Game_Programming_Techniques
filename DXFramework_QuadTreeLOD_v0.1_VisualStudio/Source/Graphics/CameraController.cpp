// ============================================================================
// CameraController.cpp
// ============================================================================

#include "Graphics/CameraController.h"

#include "Graphics/Camera.h"
#include "Input/Input.h"

#include <cmath>

void CameraController::Update(
    Camera& camera,
    const Input& input,
    float deltaTime)
{
    // ------------------------------------------------------------------------
    // 1. Orbit Rotation
    //
    // Object 자체를 돌리는 것이 아니라
    // Camera가 현재 Target 주변을 회전한다.
    // ------------------------------------------------------------------------
    if (input.IsLeftMouseDown())
    {
        const float yawDelta =
            static_cast<float>(
                input.GetMouseDeltaX()) *
            rotationSpeed_;

        // 화면에서 Mouse를 위로 Drag했을 때
        // Camera도 위쪽으로 올라가는 조작감을 위해 Y Delta의 부호를 반전한다.
        const float pitchDelta =
            -static_cast<float>(
                input.GetMouseDeltaY()) *
            rotationSpeed_;

        camera.AddOrbit(
            yawDelta,
            pitchDelta);
    }

    // ------------------------------------------------------------------------
    // 2. Mouse Wheel Zoom
    //
    // Wheel Up(+): Distance 감소 -> Zoom In
    // Wheel Down(-): Distance 증가 -> Zoom Out
    // ------------------------------------------------------------------------
    const float wheel =
        input.GetMouseWheelDelta();

    if (wheel != 0.0f)
    {
        camera.AddDistance(
            -wheel *
            zoomSpeed_);
    }

    // ------------------------------------------------------------------------
    // 3. WASD 이동 입력 수집
    //
    // Camera Position만 옮기는 것이 아니라 Target을 이동시킨다.
    // 따라서 현재 Orbit Yaw / Pitch / Distance가 유지된다.
    // ------------------------------------------------------------------------
    float forwardInput = 0.0f;
    float rightInput = 0.0f;

    if (input.IsKeyDown('W'))
    {
        forwardInput += 1.0f;
    }

    if (input.IsKeyDown('S'))
    {
        forwardInput -= 1.0f;
    }

    if (input.IsKeyDown('D'))
    {
        rightInput += 1.0f;
    }

    if (input.IsKeyDown('A'))
    {
        rightInput -= 1.0f;
    }

    if (forwardInput == 0.0f &&
        rightInput == 0.0f)
    {
        return;
    }

    // ------------------------------------------------------------------------
    // 4. 현재 Camera Yaw 기준의 수평 Forward / Right 계산
    //
    // World +Z 고정 이동이 아니라
    // 사용자가 현재 화면에서 보고 있는 방향을 기준으로 움직인다.
    // Pitch는 제외하여 XZ 평면에서만 이동한다.
    // ------------------------------------------------------------------------
    const float yaw =
        camera.GetYaw();

    const float forwardX =
        -std::sin(yaw);

    const float forwardZ =
        -std::cos(yaw);

    const float rightX =
        forwardZ;

    const float rightZ =
        -forwardX;

    // W+D처럼 대각선 입력을 했을 때
    // 직선 이동보다 sqrt(2)배 빨라지지 않도록 입력 Vector를 정규화한다.
    const float inputLength =
        std::sqrt(
            forwardInput * forwardInput +
            rightInput * rightInput);

    if (inputLength > 0.0f)
    {
        forwardInput /= inputLength;
        rightInput /= inputLength;
    }

    // FPS와 관계없이 일정한 이동 속도를 유지하기 위해 DeltaTime을 곱한다.
    const float moveAmount =
        moveSpeed_ *
        deltaTime;

    camera.MoveTarget(
        (
            forwardX * forwardInput +
            rightX * rightInput
        ) * moveAmount,
        0.0f,
        (
            forwardZ * forwardInput +
            rightZ * rightInput
        ) * moveAmount);
}

void CameraController::SetMoveSpeed(float speed)
{
    moveSpeed_ = speed;
}

void CameraController::SetRotationSpeed(
    float radiansPerPixel)
{
    rotationSpeed_ =
        radiansPerPixel;
}

void CameraController::SetZoomSpeed(float speed)
{
    zoomSpeed_ = speed;
}
