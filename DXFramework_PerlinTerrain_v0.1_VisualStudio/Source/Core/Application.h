// ============================================================================
// Application.h
// ----------------------------------------------------------------------------
// DX Framework의 최상위 실행 흐름을 관리하는 클래스.
//
// Application은 각 시스템의 초기화 순서와 메인 루프를 관리하며,
// 실제 DirectX / Input / Camera 처리는 각 전용 클래스에 맡긴다.
//
// v0.3:
// 초기화 실패 시 어느 단계에서 실패했는지 확인할 수 있도록
// 상세 오류 메시지를 보관하고 Main에서 표시할 수 있게 했다.
// ============================================================================

#pragma once

#include "Core/Timer.h"
#include "Features/PerlinTerrain/PerlinTerrain.h"
#include "Graphics/Camera.h"
#include "Graphics/CameraController.h"
#include "Graphics/ConstantBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/Renderer.h"
#include "Graphics/Shader.h"
#include "Graphics/Transform.h"
#include "Input/Input.h"
#include "Platform/Window.h"

#include <DirectXMath.h>
#include <Windows.h>

#include <string>

class Application
{
public:
    // 프로그램에 필요한 모든 시스템을 순서대로 초기화한다.
    bool Initialize(HINSTANCE hInstance);

    // Window가 종료될 때까지 Update / Render를 반복한다.
    int Run();

    // Initialize()가 false를 반환했을 때
    // 어느 단계에서 실패했는지 설명하는 문자열을 반환한다.
    const std::wstring& GetLastErrorMessage() const;

private:
    void HandleWindowMessage(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    void Update(float deltaTime);
    void Render();

    // 공용 Mesh 시스템 검증용 Cube를 생성한다.
    bool CreateTestCube();

    // 초기화 실패 내용을 한 곳에서 저장하고 Debug Output에도 출력한다.
    bool SetInitializationError(
        const wchar_t* stage,
        const std::wstring& detail = L"");

private:
    struct CBTransform
    {
        DirectX::XMFLOAT4X4 WorldViewProjection;
    };

private:
    Window window_;
    Input input_;
    Timer timer_;

    Renderer renderer_;

    Camera camera_;
    CameraController cameraController_;

    Mesh testCube_;
    Shader basicShader_;
    ConstantBuffer<CBTransform> transformBuffer_;
    Transform cubeTransform_;

    // --------------------------------------------------------------------
    // 선택 기능: Perlin Terrain
    //
    // Framework의 Graphics 구현을 수정하지 않고 독립 Feature 객체 하나만
    // Application에 연결한다. 기능을 제거하려면 이 멤버와 Initialize/Render
    // 호출만 제거하면 된다.
    // --------------------------------------------------------------------
    PerlinTerrain perlinTerrain_;

    bool initialized_ = false;

    // 마지막 초기화 오류.
    // 초기화에 성공하면 빈 문자열을 유지한다.
    std::wstring lastErrorMessage_;
};
