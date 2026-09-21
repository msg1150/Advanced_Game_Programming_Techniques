// ============================================================================
// Application.h — 대형 Disk Terrain 하나만 초기화하고 표시하는 진입 지점.
// 이전 Showcase의 Feature 객체·GPU Mesh를 생성하지 않는다.
// ============================================================================
#pragma once
#include "Core/Timer.h"
#include "Features/DiskTerrainStreaming/DiskTerrainManager.h"
#include "Graphics/Camera.h"
#include "Graphics/CameraController.h"
#include "Graphics/DebugTextRenderer.h"
#include "Graphics/Renderer.h"
#include "Input/Input.h"
#include "Platform/Window.h"
#include <Windows.h>
#include <string>
class Application
{
public:
    bool Initialize(HINSTANCE instance);
    int Run();
    const std::wstring& GetLastErrorMessage() const;
private:
    void HandleWindowMessage(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
    void Update(float deltaTime);
    void Render();
    bool Fail(const wchar_t* stage,const std::wstring& message);
    Window window_;
    Input input_;
    Timer timer_;
    Renderer renderer_;
    Camera camera_;
    CameraController cameraController_;
    DebugTextRenderer debugText_;
    DiskTerrainManager terrain_;
    bool debugPanel_=false,miniMap_=true,initialized_=false;
    std::wstring lastError_;
};
