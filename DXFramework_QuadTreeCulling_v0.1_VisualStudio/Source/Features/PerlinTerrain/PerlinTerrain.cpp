// ============================================================================
// PerlinTerrain.cpp
// ============================================================================

#include "Features/PerlinTerrain/PerlinTerrain.h"

#include <sstream>

using namespace DirectX;

bool PerlinTerrain::Initialize(
    ID3D11Device* device,
    const std::filesystem::path& shaderDirectory,
    const PerlinTerrainSettings& settings)
{
    initialized_ = false;
    lastErrorMessage_.clear();

    if (!device)
    {
        lastErrorMessage_ =
            L"PerlinTerrain::Initialize에 전달된 ID3D11Device가 nullptr입니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 1. CPU Terrain Mesh Data 생성
    // ------------------------------------------------------------------------
    PerlinTerrainMeshData meshData;
    std::wstring generatorError;

    if (!PerlinTerrainGenerator::Generate(
            settings,
            meshData,
            generatorError))
    {
        lastErrorMessage_ =
            L"Perlin Terrain Mesh 생성 실패\n" +
            generatorError;

        return false;
    }

    // ------------------------------------------------------------------------
    // 2. 생성한 CPU 데이터를 기존 Framework Mesh로 GPU에 업로드
    //
    // Terrain 전용 VertexBuffer 구현을 별도로 만들지 않고
    // 이미 검증된 Mesh 시스템을 그대로 재사용한다.
    // ------------------------------------------------------------------------
    if (!mesh_.Initialize(
            device,
            meshData.Vertices,
            meshData.Indices))
    {
        lastErrorMessage_ =
            L"Perlin Terrain Vertex/Index Buffer 생성에 실패했습니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 3. Terrain 전용 Shader 초기화
    // ------------------------------------------------------------------------
    if (!shader_.Initialize(
            device,
            shaderDirectory / L"TerrainVS.hlsl",
            shaderDirectory / L"TerrainPS.hlsl"))
    {
        std::wstringstream stream;

        stream
            << L"Perlin Terrain Shader 초기화 실패\n\n"
            << L"Shader Directory:\n"
            << shaderDirectory.wstring()
            << L"\n\n"
            << shader_.GetLastErrorMessage();

        lastErrorMessage_ =
            stream.str();

        return false;
    }

    // ------------------------------------------------------------------------
    // 4. Terrain Transform Constant Buffer 생성
    // ------------------------------------------------------------------------
    if (!transformBuffer_.Initialize(device))
    {
        lastErrorMessage_ =
            L"Perlin Terrain Transform Constant Buffer 생성에 실패했습니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 5. Terrain 전용 Wireframe Rasterizer State 생성
    // ------------------------------------------------------------------------
    if (!CreateWireframeRasterizerState(device))
    {
        lastErrorMessage_ =
            L"Perlin Terrain Wireframe Rasterizer State 생성에 실패했습니다.";

        return false;
    }

    initialized_ = true;
    return true;
}

void PerlinTerrain::Render(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection)
{
    if (!initialized_ ||
        !context)
    {
        return;
    }

    // ------------------------------------------------------------------------
    // 현재 Renderer의 Rasterizer State를 임시로 보관한다.
    //
    // Terrain은 Wireframe이 필요하지만,
    // 이 State를 그대로 남겨 두면 이후 그려지는 다른 Mesh까지
    // Wireframe으로 바뀌게 된다.
    //
    // 따라서 Terrain Draw 동안만 State를 교체하고
    // Draw가 끝난 즉시 원래 상태를 복구한다.
    // ------------------------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>
        previousRasterizerState;

    context->RSGetState(
        previousRasterizerState.GetAddressOf());

    context->RSSetState(
        wireframeRasterizerState_.Get());

    // Terrain 전용 Shader와 공용 Mesh Buffer를 바인딩한다.
    shader_.Bind(context);
    mesh_.Bind(context);

    // 기존 Framework와 동일한 World * View * Projection 흐름을 사용한다.
    const XMMATRIX world =
        transform_.GetWorldMatrix();

    CBTransform transformData = {};

    XMStoreFloat4x4(
        &transformData.WorldViewProjection,
        XMMatrixTranspose(
            world *
            view *
            projection));

    transformBuffer_.Update(
        context,
        transformData);

    transformBuffer_.BindVS(
        context,
        0);

    mesh_.Draw(context);

    // Terrain Feature가 변경했던 Pipeline State를 원래 값으로 되돌린다.
    context->RSSetState(
        previousRasterizerState.Get());
}

bool PerlinTerrain::IsInitialized() const
{
    return initialized_;
}

const std::wstring& PerlinTerrain::GetLastErrorMessage() const
{
    return lastErrorMessage_;
}

Transform& PerlinTerrain::GetTransform()
{
    return transform_;
}

const Transform& PerlinTerrain::GetTransform() const
{
    return transform_;
}

bool PerlinTerrain::CreateWireframeRasterizerState(
    ID3D11Device* device)
{
    D3D11_RASTERIZER_DESC rasterizerDesc = {};

    // Grid Triangle 구조가 눈에 보이도록 면을 채우지 않고 선으로 그린다.
    rasterizerDesc.FillMode =
        D3D11_FILL_WIREFRAME;

    // Terrain 아래쪽의 Back Face는 렌더링하지 않는다.
    rasterizerDesc.CullMode =
        D3D11_CULL_BACK;

    // 기존 Framework Renderer와 동일한 Front Face 규칙을 사용한다.
    rasterizerDesc.FrontCounterClockwise =
        FALSE;

    rasterizerDesc.DepthClipEnable =
        TRUE;

    const HRESULT result =
        device->CreateRasterizerState(
            &rasterizerDesc,
            wireframeRasterizerState_.GetAddressOf());

    return SUCCEEDED(result);
}
