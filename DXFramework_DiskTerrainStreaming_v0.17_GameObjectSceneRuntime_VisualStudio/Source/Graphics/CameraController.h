// ============================================================================
// CameraController.h
// ----------------------------------------------------------------------------
// Input 시스템의 값을 해석하여 Orbit Camera를 조작합니다.
// UI가 Wheel을 소비한 프레임에는 allowZoom=false로 Camera Zoom만 차단할 수 있습니다.
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
        float deltaTime,
        bool allowOrbit = true,
        bool allowMove = true,
        bool allowZoom = true);

    void SetMoveSpeed(float speed);
    void SetRotationSpeed(float radiansPerPixel);
    void SetZoomSpeed(float speed);
private:
    float moveSpeed_ = 5.0f;
    float rotationSpeed_ = 0.006f;
    float zoomSpeed_ = 1.0f;
};
