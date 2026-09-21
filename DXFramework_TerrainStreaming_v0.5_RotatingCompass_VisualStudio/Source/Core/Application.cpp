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
    // 11. 선택 Feature: QuadTree LOD Terrain 초기화
    //
    // 이전 단계의 Height/Slope Texture Splatting 설정을 그대로 재사용한다.
    // 기존 QuadTreeCulling Feature 자체는 수정하지 않고,
    // Source/Features/QuadTreeLOD의 독립 Feature를 새로 연결한다.
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

    QuadTreeLODTerrainSettings lodTerrainSettings = {};

    // 129x129 Vertex = 128x128 Cell 기준.
    //
    // 128 -> 64 -> 32 -> 16 -> 8
    //
    // LeafCellSize 8이면:
    // LOD0 = 8 Cell 영역  : 가장 세밀함
    // LOD1 = 16 Cell 영역
    // LOD2 = 32 Cell 영역
    // LOD3 = 64 Cell 영역
    // LOD4 = 128 Cell 영역: 가장 성김
    lodTerrainSettings.LOD.LeafCellSize =
        8u;

    // 값이 클수록 Camera가 더 멀리 있어도 Child로 세분화된다.
    lodTerrainSettings.LOD.SplitDistanceFactor =
        3.0f;

    // 서로 다른 LOD 경계의 Crack을 Feature Local Skirt로 가린다.
    lodTerrainSettings.LOD.SkirtDepth =
        0.18f;

    lodTerrainSettings.EnableCulling =
        true;

    lodTerrainSettings.EnableLOD =
        true;

    if (!quadTreeLODTerrain_.Initialize(
            renderer_.GetDevice(),
            renderer_.GetContext(),
            shaderRoot / L"TextureSplatting",
            heightMapImagePath,
            heightMapTerrainSettings,
            splatMaterialDesc,
            lodTerrainSettings))
    {
        return SetInitializationError(
            L"QuadTree LOD Terrain 초기화",
            quadTreeLODTerrain_.GetLastErrorMessage());
    }


    // ------------------------------------------------------------------------
    // 12. 선택 Feature: Triplanar Terrain 초기화
    //
    // 기존 HeightMap Geometry / QuadTree LOD는 수정하지 않는다.
    // 같은 Terrain 데이터와 같은 Grass/Rock/Snow Texture를 재사용하되,
    // 별도 Triplanar Material / Shader를 사용한다.
    // ------------------------------------------------------------------------
    TriplanarMaterialDesc triplanarMaterialDesc = {};

    triplanarMaterialDesc.GrassTexturePath =
        assetRoot / L"Textures" / L"Terrain" / L"Grass.png";

    triplanarMaterialDesc.RockTexturePath =
        assetRoot / L"Textures" / L"Terrain" / L"Rock.png";

    triplanarMaterialDesc.SnowTexturePath =
        assetRoot / L"Textures" / L"Terrain" / L"Snow.png";

    // World Unit 기준 반복 밀도.
    triplanarMaterialDesc.ProjectionScale =
        2.0f;

    // X/Y/Z Projection 경계를 얼마나 선명하게 선택할지 결정한다.
    triplanarMaterialDesc.BlendSharpness =
        4.0f;

    // 이전 Texture Splatting의 Height / Slope 규칙과 동일하게 맞춘다.
    triplanarMaterialDesc.GrassFadeStartHeight =
        0.55f;

    triplanarMaterialDesc.GrassFadeEndHeight =
        1.35f;

    triplanarMaterialDesc.RockSlopeStart =
        0.18f;

    triplanarMaterialDesc.RockSlopeEnd =
        0.48f;

    triplanarMaterialDesc.SnowStartHeight =
        1.25f;

    triplanarMaterialDesc.SnowFullHeight =
        2.35f;

    TriplanarTerrainSettings triplanarTerrainSettings = {};

    triplanarTerrainSettings.LOD =
        lodTerrainSettings.LOD;

    triplanarTerrainSettings.EnableCulling =
        lodTerrainSettings.EnableCulling;

    triplanarTerrainSettings.EnableLOD =
        lodTerrainSettings.EnableLOD;

    if (!triplanarTerrain_.Initialize(
            renderer_.GetDevice(),
            renderer_.GetContext(),
            shaderRoot / L"TriplanarTerrain",
            heightMapImagePath,
            heightMapTerrainSettings,
            triplanarMaterialDesc,
            triplanarTerrainSettings))
    {
        return SetInitializationError(
            L"Triplanar Terrain 초기화",
            triplanarTerrain_.GetLastErrorMessage());
    }

    // ------------------------------------------------------------------------
    // 13. Terrain Chunk System: 전체 HeightMap을 독립 Chunk로 분할한다.
    // HeightMap Generator / QuadTreeLOD / TriplanarMaterial은 변경 없이 재사용.
    // ------------------------------------------------------------------------
    TerrainChunkSettings chunkSettings = {};
    chunkSettings.CellsPerChunk = 32u; // 129x129 Vertex -> 4x4 = 16 Chunks.
    chunkSettings.LOD = lodTerrainSettings.LOD;
    chunkSettings.EnableCulling = lodTerrainSettings.EnableCulling;
    chunkSettings.EnableLOD = lodTerrainSettings.EnableLOD;

    if (!terrainChunkManager_.Initialize(
            renderer_.GetDevice(),
            renderer_.GetContext(),
            heightMapImagePath,
            heightMapTerrainSettings,
            shaderRoot / L"TriplanarTerrain",
            triplanarMaterialDesc,
            chunkSettings))
    {
        return SetInitializationError(
            L"Terrain Chunk 초기화",
            terrainChunkManager_.GetLastErrorMessage());
    }

// ------------------------------------------------------------------------
// 12. Showcase 화면 표시 토글 등록
//
// 숫자키 1~0은 고정 슬롯이다.
// 현재 구현된 항목만 우선 배정하고 나머지는 예약 슬롯으로 남긴다.
// ------------------------------------------------------------------------
    // ------------------------------------------------------------------------
    // 14. GPU Mesh Streaming: 독립 Feature.
    // 기존 Chunk Manager를 변경하지 않고 HeightMap/LOD/Material을 재사용한다.
    // ------------------------------------------------------------------------
    TerrainStreamingSettings streamingSettings = {};
    streamingSettings.CellsPerChunk = chunkSettings.CellsPerChunk;
    streamingSettings.LOD = chunkSettings.LOD;
    streamingSettings.EnableCulling = chunkSettings.EnableCulling;
    streamingSettings.EnableLOD = chunkSettings.EnableLOD;
    streamingSettings.LoadRadius = 1.9f;
    streamingSettings.UnloadRadius = 2.9f;
    streamingSettings.MaxUploadsPerFrame = 2u;
    streamingSettings.EnableStreaming = true;

    if (!terrainStreamingManager_.Initialize(
            renderer_.GetDevice(), renderer_.GetContext(),
            heightMapImagePath, heightMapTerrainSettings,
            shaderRoot / L"TriplanarTerrain",
            triplanarMaterialDesc, streamingSettings))
    {
        return SetInitializationError(L"Terrain Streaming 초기화",
                                      terrainStreamingManager_.GetLastErrorMessage());
    }

showcaseVisibilityController_.SetSlot(
    0u,
    L"Cube",
    false);

showcaseVisibilityController_.SetSlot(
    1u,
    L"Perlin Terrain",
    false);

showcaseVisibilityController_.SetSlot(
    2u,
    L"HeightMap Terrain",
    false);

showcaseVisibilityController_.SetSlot(
    3u,
    L"QuadTree LOD Terrain",
    false);

showcaseVisibilityController_.SetSlot(
    4u,
    L"Triplanar Terrain",
    false);

showcaseVisibilityController_.SetSlot(
    5u,
    L"Terrain Chunks",
    false);

showcaseVisibilityController_.SetSlot(
    6u,
    L"GPU Streaming",
    true);

    // 최신 기능인 Triplanar Terrain을 World 원점에 두어
    // 프로그램 실행 직후 바로 확인할 수 있게 한다.
    terrainStreamingManager_.GetTransform().Position = { 0.0f, 0.0f, 0.0f };

    terrainChunkManager_.GetTransform().Position =
    {
        0.0f,
        0.0f,
        0.0f
    };

    triplanarTerrain_.GetTransform().Position =
    {
        10.0f,
        0.0f,
        0.0f
    };

    // 이전 단계 Terrain은 숫자키로 필요할 때만 켜서 비교한다.
    quadTreeLODTerrain_.GetTransform().Position =
    {
        20.0f,
        0.0f,
        0.0f
    };

    perlinTerrain_.GetTransform().Position =
    {
        -10.0f,
        0.0f,
        0.0f
    };

    heightMapTerrain_.GetTransform().Position =
    {
        30.0f,
        0.0f,
        0.0f
    };

    cubeTransform_.Position =
    {
        -14.0f,
        1.0f,
        0.0f
    };

    terrainStreamingManager_.UpdateStreaming(camera_.GetTarget());

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
    // F1 : Frustum Culling ON / OFF
    // ------------------------------------------------------------------------
    if (input_.IsKeyPressed(VK_F1))
    {
        const bool nextCullingState =
            !terrainStreamingManager_.IsCullingEnabled();

        // 두 Terrain을 같은 상태로 유지해 비교할 때 혼동이 없게 한다.
        triplanarTerrain_.SetCullingEnabled(
            nextCullingState);

        quadTreeLODTerrain_.SetCullingEnabled(
            nextCullingState);

        terrainChunkManager_.SetCullingEnabled(nextCullingState);
        terrainStreamingManager_.SetCullingEnabled(nextCullingState);
    }

    // ------------------------------------------------------------------------
    // F2 : QuadTree Distance LOD ON / OFF
    //
    // LOD OFF 상태에서는 보이는 영역을 Leaf(LOD0)까지 내려가
    // 원본 Grid 해상도로 렌더링한다.
    // ------------------------------------------------------------------------
    if (input_.IsKeyPressed(VK_F2))
    {
        const bool nextLODState =
            !terrainStreamingManager_.IsLODEnabled();

        triplanarTerrain_.SetLODEnabled(
            nextLODState);

        quadTreeLODTerrain_.SetLODEnabled(
            nextLODState);

        terrainChunkManager_.SetLODEnabled(nextLODState);
        terrainStreamingManager_.SetLODEnabled(nextLODState);
    }

    // ------------------------------------------------------------------------
    // 숫자키 1~0 : Showcase 항목 표시 ON / OFF
    // ------------------------------------------------------------------------
    showcaseVisibilityController_.Update(
        input_);

    // F3: Tooltip(Debug Overlay) 전체 표시 전환. 기본 OFF.
    if (input_.IsKeyPressed(VK_F3))
    {
        showDebugOverlay_ = !showDebugOverlay_;
    }

    // F4: Terrain Chunk 경계선 시각화 ON / OFF
    if (input_.IsKeyPressed(VK_F4))
    {
        const bool visible = !terrainStreamingManager_.IsChunkBordersVisible();
        terrainChunkManager_.SetChunkBoundsVisible(visible);
        terrainStreamingManager_.SetChunkBordersVisible(visible);
    }

    // F5: Streaming ON/OFF. OFF는 기존과 같이 모든 Chunk를 점진적으로 로드.
    if (input_.IsKeyPressed(VK_F5))
        terrainStreamingManager_.SetStreamingEnabled(
            !terrainStreamingManager_.IsStreamingEnabled());

    // F6: Streaming Load / Unload 판정 반경선 표시 ON / OFF
    if (input_.IsKeyPressed(VK_F6))
    {
        terrainStreamingManager_.SetStreamingRangeVisible(
            !terrainStreamingManager_.IsStreamingRangeVisible());
    }

    cameraController_.Update(camera_, input_, deltaTime);
    // Camera Target 이동 후 이번 프레임의 원하는 Chunk를 갱신한다.
    terrainStreamingManager_.UpdateStreaming(camera_.GetTarget());
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

    renderer_.BeginFrame(
        clearColor);

    const XMMATRIX view =
        camera_.GetViewMatrix();

    const XMMATRIX projection =
        camera_.GetProjectionMatrix();

    // ------------------------------------------------------------------------
    // [1] Test Cube
    // ------------------------------------------------------------------------
    if (showcaseVisibilityController_.IsVisible(
            0u))
    {
        basicShader_.Bind(
            renderer_.GetContext());

        testCube_.Bind(
            renderer_.GetContext());

        const XMMATRIX world =
            cubeTransform_.GetWorldMatrix();

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
            0u);

        testCube_.Draw(
            renderer_.GetContext());
    }

    // ------------------------------------------------------------------------
    // [2] Perlin Terrain
    // ------------------------------------------------------------------------
    if (showcaseVisibilityController_.IsVisible(
            1u))
    {
        perlinTerrain_.Render(
            renderer_.GetContext(),
            view,
            projection);
    }

    // ------------------------------------------------------------------------
    // [3] HeightMap Terrain
    // ------------------------------------------------------------------------
    if (showcaseVisibilityController_.IsVisible(
            2u))
    {
        heightMapTerrain_.Render(
            renderer_.GetContext(),
            view,
            projection);
    }

    // ------------------------------------------------------------------------
    // [4] 이전 QuadTree LOD + UV Texture Splatting Terrain
    // ------------------------------------------------------------------------
    const bool isQuadTreeLODVisible =
        showcaseVisibilityController_.IsVisible(
            3u);

    if (isQuadTreeLODVisible)
    {
        quadTreeLODTerrain_.Render(
            renderer_.GetContext(),
            view,
            projection,
            camera_.GetPosition());
    }

    // ------------------------------------------------------------------------
    // [5] 새 Triplanar + QuadTree LOD Terrain
    // ------------------------------------------------------------------------
    const bool isTriplanarVisible =
        showcaseVisibilityController_.IsVisible(
            4u);

    if (isTriplanarVisible)
    {
        triplanarTerrain_.Render(
            renderer_.GetContext(),
            view,
            projection,
            camera_.GetPosition());
    }

    // ------------------------------------------------------------------------
    // [6] 신규 Terrain Chunk System
    // ------------------------------------------------------------------------
    const bool isChunkVisible =
        showcaseVisibilityController_.IsVisible(5u);
    if (isChunkVisible)
    {
        terrainChunkManager_.Render(
            renderer_.GetContext(),
            view,
            projection,
            camera_.GetPosition());
    }

    // [7] 독립 GPU Streaming Showcase
    const bool isStreamingVisible = showcaseVisibilityController_.IsVisible(6u);
    if (isStreamingVisible)
    {
        terrainStreamingManager_.Render(
            renderer_.GetContext(), view, projection, camera_.GetPosition());
    }

    // ------------------------------------------------------------------------
    // F3 Debug Overlay. 기본은 짧은 1줄 안내만 표시한다.
    // 숨김 상태에서는 거대한 반투명 박스를 그리지 않는다.
    // 기존 Direct2D Renderer는 compact 표시 옵션만 추가한다.
    // ------------------------------------------------------------------------
    if (!showDebugOverlay_)
    {
        debugTextRenderer_.DrawTextBlock(
            renderer_.GetContext(),
            isStreamingVisible
                ? L"[F3] Info  [F6] Map"
                : L"[F3] Info",
            true); // compact
    }
    else
    {
        std::wstringstream text;
        text << L"[F3] Hide Debug Info\n"
             << showcaseVisibilityController_.BuildDebugText()
             << L"\n[F1] Culling: "
             << (terrainChunkManager_.IsCullingEnabled() ? L"ON" : L"OFF")
             << L"    [F2] LOD: "
             << (terrainChunkManager_.IsLODEnabled() ? L"ON" : L"OFF")
             << L"    [F4] Border: "
             << (terrainChunkManager_.IsChunkBoundsVisible() ? L"ON" : L"OFF");

        if (isStreamingVisible)
        {
            const TerrainStreamingStats& st = terrainStreamingManager_.GetStats();
            text << L"\n[F5] Streaming: "
                 << (terrainStreamingManager_.IsStreamingEnabled() ? L"ON" : L"OFF")
                 << L"  [F4] Borders: "
                 << (terrainStreamingManager_.IsChunkBordersVisible() ? L"ON" : L"OFF")
                 << L"  [F6] Mini Map: "
                 << (terrainStreamingManager_.IsStreamingRangeVisible() ? L"ON" : L"OFF")
                 << L"\nMap: Green circle=Load 1.9 / Orange circle=Keep 2.9"
                 << L"\nCells: Teal=Loaded  Yellow=Pending  Gray=Unloaded"
                 << L"\nWhite cross=Camera Target (not Camera position)"
                 << L"\nChunks Loaded / Total: " << st.LoadedChunks << L" / " << st.TotalChunks
                 << L"  Desired: " << st.DesiredChunks << L"  Pending: " << st.PendingChunks
                 << L"\nChunks Visible: " << st.VisibleChunks << L"  Culled: " << st.CulledChunks
                 << L"  Draw Calls: " << st.DrawCalls
                 << L"\nSurface Triangles: " << st.RenderedSurfaceTriangles
                 << L" / " << st.FullResolutionSurfaceTriangles
                 << L"\nResident Mesh Buffer: " << (st.ResidentMeshBytes / 1024u) << L" KiB";
            if (!terrainStreamingManager_.GetLastErrorMessage().empty())
                text << L"\nStreaming Error: " << terrainStreamingManager_.GetLastErrorMessage();
        }
        else if (isChunkVisible)
        {
            const TerrainChunkStats& st = terrainChunkManager_.GetStats();
            text << L"\nStats: Terrain Chunks"
                 << L"\nChunks Visible / Total: " << st.VisibleChunks
                 << L" / " << st.TotalChunks
                 << L"    Culled: " << st.CulledChunks
                 << L"\nActive Nodes: " << st.ActiveNodes
                 << L"    Draw Calls: " << st.DrawCalls
                 << L"\nSurface Triangles: " << st.RenderedSurfaceTriangles
                 << L" / " << st.FullResolutionSurfaceTriangles;
        }
        else
        {
            const QuadTreeLODStats* st = nullptr;
            const wchar_t* source = nullptr;
            if (isTriplanarVisible)
            {
                st = &triplanarTerrain_.GetStats();
                source = L"Triplanar Terrain";
            }
            else if (isQuadTreeLODVisible)
            {
                st = &quadTreeLODTerrain_.GetStats();
                source = L"QuadTree LOD Terrain";
            }
            if (st)
            {
                text << L"\nStats: " << source
                     << L"\nActive Nodes: " << st->ActiveNodes
                     << L"    Draw Calls: " << st->DrawCalls
                     << L"\nSurface Triangles: " << st->RenderedSurfaceTriangles
                     << L" / " << st->FullResolutionSurfaceTriangles;
            }
            else
            {
                text << L"\nNo LOD Terrain visible.";
            }
        }
        debugTextRenderer_.DrawTextBlock(
            renderer_.GetContext(), text.str(), false);
    }

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
