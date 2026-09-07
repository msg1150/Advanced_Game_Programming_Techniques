// ============================================================================
// CameraController.h
// ----------------------------------------------------------------------------
// Input 시스템의 값을 해석하여 Orbit Camera를 조작한다.
//
// 현재 조작:
// - W / A / S / D : Camera 방향 기준 Target 이동
// - Left Mouse + Drag : Orbit 회전
// - Mouse Wheel : Zoom
//
// Camera와 Input을 분리해 둬서
// 이후 다른 카메라 컨트롤 방식을 쉽게 추가할 수 있다.
// ============================================================================

#pragma once

class Camera;
class Input;

class CameraController
{
public:
    void Update(
        Camera& camera,
        const Input& input,
        float deltaTime);

    void SetMoveSpeed(float speed);

    // Mouse 1 Pixel 이동당 적용할 Radian 값.
    void SetRotationSpeed(float radiansPerPixel);

    // Mouse Wheel 한 칸당 Camera Distance 변화량.
    void SetZoomSpeed(float speed);

private:
    float moveSpeed_ = 5.0f;
    float rotationSpeed_ = 0.006f;
    float zoomSpeed_ = 1.0f;
};
