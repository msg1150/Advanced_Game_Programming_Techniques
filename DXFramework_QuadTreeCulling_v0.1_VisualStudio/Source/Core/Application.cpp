// ============================================================================
// Application.cpp
// ----------------------------------------------------------------------------
// DX Framework 전체 초기화 순서와 메인 루프를 구현한다.
//
// v0.3에서는 각 초기화 단계의 실패 원인을 기록한다.
// 따라서 "초기화 실패"만 표시되는 것이 아니라
// Window / Renderer / Shader / ConstantBuffer / Mesh 중
// 어느 단계가 실패했는지 바로 확인할 수 있다.
// ============================================================================

#include "Core/Application.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <vector>

using namespace DirectX;

namespace
{
    // 실행 중인 DXFramework.exe의 실제 폴더를 얻는다.
    // Shader 파일은 이 경로를 기준으로 찾는다.
    std::filesystem::path GetExecutableDirectory()
    {
        wchar_t buffer[MAX_PATH] = {};

        const DWORD length =
            GetModuleFileNameW(
                nullptr,
                buffer,
                MAX_PATH);

        if (length == 0)
        {
            return std::filesystem::current_path();
        }

        return std::filesystem::path(buffer).parent_path();
    }
}

bool Application::Initialize(HINSTANCE hInstance)
{
    // 이전 실행의 오류 문자열이 남지 않도록 초기화한다.
    lastErrorMessage_.clear();

    // ------------------------------------------------------------------------
    // 1. Window Message Handler 연결
    // ------------------------------------------------------------------------
    window_.SetMessageHandler(
        [this](
            HWND hwnd,
            UINT message,
            WPARAM wParam,
            LPARAM lParam)
        {
            HandleWindowMessage(
                hwnd,
                message,
                wParam,
                lParam);
        });

    // ------------------------------------------------------------------------
    // 2. Win32 Window 생성
    // ------------------------------------------------------------------------
    if (!window_.Initialize(
            hInstance,
            1280,
            720,
            L"DirectX Framework - Cube Test"))
    {
        return SetInitializationError(
            L"Window 초기화",
            L"Win32 Window 생성에 실패했습니다.");
    }

    // ------------------------------------------------------------------------
    // 3. Direct3D 11 Renderer 초기화
    // ------------------------------------------------------------------------
    if (!renderer_.Initialize(
            window_.GetHandle(),
            window_.GetClientWidth(),
            window_.GetClientHeight()))
    {
        return SetInitializationError(
            L"Renderer 초기화",
            L"Direct3D 11 Device / SwapChain / RenderTarget 생성 중 실패했습니다.");
    }

    // ------------------------------------------------------------------------
    // 4. Debug Text Overlay 초기화
    //
    // Direct2D / DirectWrite를 이용해 F1 조작법과 QuadTree 통계를
    // 실제 BackBuffer 위에 표시한다.
    // ------------------------------------------------------------------------
    if (!debugTextRenderer_.Initialize(
            renderer_.GetSwapChain()))
    {
        return SetInitializationError(
            L"Debug Text Overlay 초기화",
            debugTextRenderer_.GetLastErrorMessage());
    }

    // ------------------------------------------------------------------------
    // 5. Perspective Camera 설정
    // ------------------------------------------------------------------------
    const float aspectRatio =
        static_cast<float>(window_.GetClientWidth()) /
        static_cast<float>(
            std::max(
                1u,
                window_.GetClientHeight()));

    camera_.SetPerspective(
        XMConvertToRadians(60.0f),
        aspectRatio,
        0.1f,
        1000.0f);

    camera_.SetTarget(
        {
            0.0f,
            0.0f,
            0.0f
        });

    camera_.SetOrbit(
        XMConvertToRadians(180.0f),
        XMConvertToRadians(25.0f),
        8.0f);

    // ------------------------------------------------------------------------
    // 6. Basic Shader 초기화
    //
    // Shader는 실행 파일 옆의
    // Shaders/Basic/BasicVS.hlsl, BasicPS.hlsl을 찾는다.
    // ------------------------------------------------------------------------
    const auto executableRoot =
        GetExecutableDirectory();

    const auto shaderRoot =
        executableRoot /
        L"Shaders";

    const auto assetRoot =
        executableRoot /
        L"Assets";

    if (!basicShader_.Initialize(
            renderer_.GetDevice(),
            shaderRoot / L"Basic" / L"BasicVS.hlsl",
            shaderRoot / L"Basic" / L"BasicPS.hlsl"))
    {
        std::wstringstream detail;

        detail
            << L"Shader 초기화에 실패했습니다.\n\n"
            << L"검색한 Shader 폴더:\n"
            << (shaderRoot / L"Basic").wstring()
            << L"\n\n"
            << basicShader_.GetLastErrorMessage();

        return SetInitializationError(
            L"Shader 초기화",
            detail.str());
    }

    // ------------------------------------------------------------------------
    // 7. Transform Constant Buffer 생성
    // ------------------------------------------------------------------------
    if (!transformBuffer_.Initialize(
            renderer_.GetDevice()))
    {
        return SetInitializationError(
            L"Constant Buffer 초기화",
            L"Transform용 Direct3D Constant Buffer 생성에 실패했습니다.");
    }

    // ------------------------------------------------------------------------
    // 8. 테스트 Cube Mesh 생성
    // ------------------------------------------------------------------------
    if (!CreateTestCube())
    {
        return SetInitializationError(
            L"Cube Mesh 초기화",
            L"테스트 Cube의 Vertex Buffer 또는 Index Buffer 생성에 실패했습니다.");
    }

    // ------------------------------------------------------------------------
    // 9. 선택 Feature: Perlin Terrain 초기화
    //
    // Perlin Terrain은 Renderer / Camera / Mesh 구현 자체를 수정하지 않는다.
    // 기존 Framework가 제공하는 Device와 공용 Graphics 클래스를 사용하며
    // 자체 Mesh / Shader / Wireframe State를 Feature 내부에 보관한다.
    //
    // 이 기능을 제거하려면:
    // - 아래 Initialize 호출
    // - Application.h의 PerlinTerrain 멤버
    // - Render()의 perlinTerrain_.Render 호출
    // - Source/Features/PerlinTerrain 및 Shaders/PerlinTerrain
    // 만 제거하면 된다.
    // ------------------------------------------------------------------------
    const PerlinTerrainSettings terrainSettings = {};

    if (!perlinTerrain_.Initialize(
            renderer_.GetDevice(),
            shaderRoot / L"PerlinTerrain",
            terrainSettings))
    {
        return SetInitializationError(
            L"Perlin Terrain 초기화",
            perlinTerrain_.GetLastErrorMessage());
    }

    // ------------------------------------------------------------------------
    // 10. 선택 Feature: HeightMap Terrain 초기화
    //
    // 실사용 HeightMap 경로와 높이 범위는 여기서 조절한다.
    // 앞으로 HeightMap Showcase를 여러 개 만들 경우에도
    // Feature 구현을 건드리지 말고 이 초기화 값만 교체하는 쪽이 좋다.
    // ------------------------------------------------------------------------
    HeightMapTerrainSettings heightMapTerrainSettings = {};
    heightMapTerrainSettings.CellSize = 0.05f;
    heightMapTerrainSettings.MinHeight = -1.0f;
    heightMapTerrainSettings.MaxHeight = 3.0f;

    const auto heightMapImagePath =
        assetRoot / L"HeightMaps" / L"HeightMap_Test.png";

    if (!heightMapTerrain_.Initialize(
            renderer_.GetDevice(),
            shaderRoot / L"HeightMapTerrain",
            heightMapImagePath,
            heightMapTerrainSettings))
    {
        return SetInitializationError(
            L"HeightMap Terrain 초기화",
            heightMapTerrain_.GetLastErrorMessage());
    }

    // ------------------------------------------------------------------------
    // 11. 선택 Feature: QuadTree Culling Terrain 초기화
    //
    // 이전 Texture Splatting 단계의 Texture / Height / Slope 설정을 그대로 쓰고,
    // Grid만 QuadTree Leaf 단위 Index Range로 재구성한다.
    // ------------------------------------------------------------------------
    TextureSplatMaterialDesc splatMaterialDesc = {};

    splatMaterialDesc.Layer0TexturePath =
        assetRoot / L"Textures" / L"Terrain" / L"Grass.png";

    splatMaterialDesc.Layer1TexturePath =
        assetRoot / L"Textures" / L"Terrain" / L"Rock.png";

    splatMaterialDesc.Layer2TexturePath =
        assetRoot / L"Textures" / L"Terrain" / L"Snow.png";

    splatMaterialDesc.TextureTiling =
        12.0f;

    splatMaterialDesc.GrassFadeStartHeight =
        0.55f;

    splatMaterialDesc.GrassFadeEndHeight =
        1.35f;

    splatMaterialDesc.RockSlopeStart =
        0.18f;

    splatMaterialDesc.RockSlopeEnd =
        0.48f;

    splatMaterialDesc.SnowStartHeight =
        1.25f;

    splatMaterialDesc.SnowFullHeight =
        2.35f;

    QuadTreeTerrainSettings quadTreeSettings = {};

    // 129x129 HeightMap = 128x128 Cell 기준
    // 8x8 Cell Leaf를 사용하면 최대 16x16 = 256 Leaf가 만들어진다.
    quadTreeSettings.QuadTree.LeafCellSize =
        8u;

    quadTreeSettings.EnableCulling =
        true;

    if (!quadTreeTerrain_.Initialize(
            renderer_.GetDevice(),
            renderer_.GetContext(),
            shaderRoot / L"TextureSplatting",
            heightMapImagePath,
            heightMapTerrainSettings,
            splatMaterialDesc,
            quadTreeSettings))
    {
        return SetInitializationError(
            L"QuadTree Culling Terrain 초기화",
            quadTreeTerrain_.GetLastErrorMessage());
    }

    // 각 테스트 대상을 좌우로 분리해 한 화면에서 비교하기 쉽게 한다.
    // 새 기능인 Texture Splat Terrain을 World 원점에 두어
    // 프로그램 실행 직후 바로 확인할 수 있게 한다.
    quadTreeTerrain_.GetTransform().Position =
    {
        0.0f,
        0.0f,
        0.0f
    };

    // 이전 단계의 기능들은 회귀 테스트용으로 좌우에 남겨둔다.
    perlinTerrain_.GetTransform().Position =
    {
        -10.0f,
        0.0f,
        0.0f
    };

    heightMapTerrain_.GetTransform().Position =
    {
        10.0f,
        0.0f,
        0.0f
    };

    cubeTransform_.Position =
    {
        -14.0f,
        1.0f,
        0.0f
    };

    timer_.Reset();

    initialized_ = true;
    return true;
}

int Application::Run()
{
    while (true)
    {
        input_.BeginFrame();

        if (!window_.ProcessMessages())
        {
            break;
        }

        timer_.Tick();

        Update(timer_.GetDeltaTime());
        Render();
    }

    return 0;
}

const std::wstring& Application::GetLastErrorMessage() const
{
    return lastErrorMessage_;
}

bool Application::SetInitializationError(
    const wchar_t* stage,
    const std::wstring& detail)
{
    std::wstringstream stream;

    stream
        << L"초기화 실패 단계: "
        << stage;

    if (!detail.empty())
    {
        stream
            << L"\n\n"
            << detail;
    }

    lastErrorMessage_ =
        stream.str();

    // Visual Studio의 Output > Debug 창에서도 같은 내용을 확인할 수 있다.
    OutputDebugStringW(
        L"[DXFramework] ");

    OutputDebugStringW(
        lastErrorMessage_.c_str());

    OutputDebugStringW(
        L"\n");

    // false를 반환하여 Initialize()의 실패 흐름을 그대로 유지한다.
    return false;
}

void Application::HandleWindowMessage(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    input_.ProcessMessage(
        hwnd,
        message,
        wParam,
        lParam);

    if (message == WM_SIZE &&
        renderer_.IsInitialized())
    {
        const UINT width =
            LOWORD(lParam);

        const UINT height =
            HIWORD(lParam);

        if (width > 0 &&
            height > 0)
        {
            // Direct2D RenderTarget이 SwapChain BackBuffer를 참조하고 있으므로
            // ResizeBuffers 전에 먼저 참조를 해제한다.
            if (debugTextRenderer_.IsInitialized())
            {
                debugTextRenderer_.PrepareForResize();
            }

            renderer_.Resize(
                width,
                height);

            if (debugTextRenderer_.IsInitialized())
            {
                debugTextRenderer_.RecreateTarget();
            }

            camera_.SetAspectRatio(
                static_cast<float>(width) /
                static_cast<float>(height));
        }
    }
}

void Application::Update(float deltaTime)
{
    // ------------------------------------------------------------------------
    // F1 : QuadTree Frustum Culling ON / OFF
    //
    // Pressed는 한 번 눌렀을 때 한 번만 true가 되므로
    // 키를 계속 누르고 있어도 매 Frame 토글되지 않는다.
    // ------------------------------------------------------------------------
    if (input_.IsKeyPressed(VK_F1))
    {
        quadTreeTerrain_.SetCullingEnabled(
            !quadTreeTerrain_.IsCullingEnabled());
    }

    cameraController_.Update(
        camera_,
        input_,
        deltaTime);
}

void Application::Render()
{
    constexpr float clearColor[4] =
    {
        0.08f,
        0.09f,
        0.11f,
        1.0f
    };

    renderer_.BeginFrame(clearColor);

    basicShader_.Bind(
        renderer_.GetContext());

    testCube_.Bind(
        renderer_.GetContext());

    const XMMATRIX world =
        cubeTransform_.GetWorldMatrix();

    const XMMATRIX view =
        camera_.GetViewMatrix();

    const XMMATRIX projection =
        camera_.GetProjectionMatrix();

    CBTransform transformData = {};

    XMStoreFloat4x4(
        &transformData.WorldViewProjection,
        XMMatrixTranspose(
            world *
            view *
            projection));

    transformBuffer_.Update(
        renderer_.GetContext(),
        transformData);

    transformBuffer_.BindVS(
        renderer_.GetContext(),
        0);

    testCube_.Draw(
        renderer_.GetContext());

    // ------------------------------------------------------------------------
    // Perlin Terrain 렌더링
    //
    // Terrain Feature는 자신의 Shader / Wireframe Rasterizer State를
    // 내부에서 설정하고, Draw가 끝나면 기존 Rasterizer State를 복구한다.
    // 따라서 Renderer의 기본 State나 다른 Mesh에 영향을 남기지 않는다.
    // ------------------------------------------------------------------------
    perlinTerrain_.Render(
        renderer_.GetContext(),
        view,
        projection);

    // HeightMap Terrain 역시 Feature 내부에서 자기 Shader / Wireframe State를
    // 관리하고 Draw 이후 기존 Rasterizer State를 복구한다.
    heightMapTerrain_.Render(
        renderer_.GetContext(),
        view,
        projection);

    // ------------------------------------------------------------------------
    // QuadTree Culling Terrain
    //
    // 기존 Height/Slope Texture Splatting은 그대로 유지하면서
    // Frustum에 보이는 QuadTree Index Range만 Draw한다.
    // ------------------------------------------------------------------------
    quadTreeTerrain_.Render(
        renderer_.GetContext(),
        view,
        projection);

    // ------------------------------------------------------------------------
    // 화면 Debug Overlay
    //
    // F1 조작법과 현재 Culling 상태/통계를 항상 표시한다.
    // ------------------------------------------------------------------------
    const QuadTreeCullingStats& cullingStats =
        quadTreeTerrain_.GetCullingStats();

    std::wstringstream debugText;

    debugText
        << L"[F1] QuadTree Culling : "
        << (
            quadTreeTerrain_.IsCullingEnabled()
            ? L"ON"
            : L"OFF")
        << L"\n"
        << L"Visible Leaves : "
        << cullingStats.VisibleLeaves
        << L" / "
        << cullingStats.TotalLeaves
        << L"    Culled Nodes : "
        << cullingStats.CulledNodes
        << L"\n"
        << L"Rendered Triangles : "
        << cullingStats.RenderedTriangles
        << L" / "
        << cullingStats.TotalTriangles
        << L"    Draw Calls : "
        << cullingStats.DrawCalls;

    debugTextRenderer_.DrawTextBlock(
        renderer_.GetContext(),
        debugText.str());

    renderer_.EndFrame();
}

bool Application::CreateTestCube()
{
    // 면마다 독립된 Normal / UV를 가질 수 있게 24개의 Vertex를 사용한다.
    const std::vector<Vertex> vertices =
    {
        // Front (+Z)
        {{-1,-1, 1}, { 0, 0, 1}, {0,1}, {0.90f,0.25f,0.25f,1}},
        {{-1, 1, 1}, { 0, 0, 1}, {0,0}, {0.90f,0.25f,0.25f,1}},
        {{ 1, 1, 1}, { 0, 0, 1}, {1,0}, {0.90f,0.25f,0.25f,1}},
        {{ 1,-1, 1}, { 0, 0, 1}, {1,1}, {0.90f,0.25f,0.25f,1}},

        // Back (-Z)
        {{ 1,-1,-1}, { 0, 0,-1}, {0,1}, {0.25f,0.75f,0.95f,1}},
        {{ 1, 1,-1}, { 0, 0,-1}, {0,0}, {0.25f,0.75f,0.95f,1}},
        {{-1, 1,-1}, { 0, 0,-1}, {1,0}, {0.25f,0.75f,0.95f,1}},
        {{-1,-1,-1}, { 0, 0,-1}, {1,1}, {0.25f,0.75f,0.95f,1}},

        // Left (-X)
        {{-1,-1,-1}, {-1, 0, 0}, {0,1}, {0.35f,0.85f,0.45f,1}},
        {{-1, 1,-1}, {-1, 0, 0}, {0,0}, {0.35f,0.85f,0.45f,1}},
        {{-1, 1, 1}, {-1, 0, 0}, {1,0}, {0.35f,0.85f,0.45f,1}},
        {{-1,-1, 1}, {-1, 0, 0}, {1,1}, {0.35f,0.85f,0.45f,1}},

        // Right (+X)
        {{ 1,-1, 1}, { 1, 0, 0}, {0,1}, {0.95f,0.75f,0.20f,1}},
        {{ 1, 1, 1}, { 1, 0, 0}, {0,0}, {0.95f,0.75f,0.20f,1}},
        {{ 1, 1,-1}, { 1, 0, 0}, {1,0}, {0.95f,0.75f,0.20f,1}},
        {{ 1,-1,-1}, { 1, 0, 0}, {1,1}, {0.95f,0.75f,0.20f,1}},

        // Top (+Y)
        {{-1, 1, 1}, { 0, 1, 0}, {0,1}, {0.65f,0.40f,0.95f,1}},
        {{-1, 1,-1}, { 0, 1, 0}, {0,0}, {0.65f,0.40f,0.95f,1}},
        {{ 1, 1,-1}, { 0, 1, 0}, {1,0}, {0.65f,0.40f,0.95f,1}},
        {{ 1, 1, 1}, { 0, 1, 0}, {1,1}, {0.65f,0.40f,0.95f,1}},

        // Bottom (-Y)
        {{-1,-1,-1}, { 0,-1, 0}, {0,1}, {0.30f,0.30f,0.35f,1}},
        {{-1,-1, 1}, { 0,-1, 0}, {0,0}, {0.30f,0.30f,0.35f,1}},
        {{ 1,-1, 1}, { 0,-1, 0}, {1,0}, {0.30f,0.30f,0.35f,1}},
        {{ 1,-1,-1}, { 0,-1, 0}, {1,1}, {0.30f,0.30f,0.35f,1}},
    };

    // v0.2에서 수정한 바깥 방향 Triangle Winding을 유지한다.
    const std::vector<std::uint32_t> indices =
    {
         0, 2, 1,   0, 3, 2,
         4, 6, 5,   4, 7, 6,
         8,10, 9,   8,11,10,
        12,14,13,  12,15,14,
        16,18,17,  16,19,18,
        20,22,21,  20,23,22,
    };

    return testCube_.Initialize(
        renderer_.GetDevice(),
        vertices,
        indices);
}
