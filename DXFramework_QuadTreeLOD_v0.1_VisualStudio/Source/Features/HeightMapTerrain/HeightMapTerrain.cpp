// ============================================================================
// HeightMapTerrain.cpp
// ============================================================================

#include "Features/HeightMapTerrain/HeightMapTerrain.h"

#include <sstream>

using namespace DirectX;

bool HeightMapTerrain::Initialize(
    ID3D11Device* device,
    const std::filesystem::path& shaderDirectory,
    const std::filesystem::path& imagePath,
    const HeightMapTerrainSettings& settings)
{
    initialized_ = false;
    lastErrorMessage_.clear();

    if (!device)
    {
        lastErrorMessage_ =
            L"HeightMapTerrain::Initialize에 전달된 ID3D11Device가 nullptr입니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 1. HeightMap 이미지 로드
    // ------------------------------------------------------------------------
    if (!heightMapImage_.Load(imagePath))
    {
        lastErrorMessage_ =
            L"HeightMap 이미지 로드 실패\n\n" +
            heightMapImage_.GetLastErrorMessage();

        return false;
    }

    // ------------------------------------------------------------------------
    // 2. HeightMap -> CPU Mesh Data 생성
    // ------------------------------------------------------------------------
    HeightMapTerrainMeshData meshData;
    std::wstring generatorError;

    if (!HeightMapTerrainGenerator::Generate(
            heightMapImage_,
            settings,
            meshData,
            generatorError))
    {
        lastErrorMessage_ =
            L"HeightMap Terrain Mesh 생성 실패\n" +
            generatorError;

        return false;
    }

    // ------------------------------------------------------------------------
    // 3. 공용 Mesh 시스템에 GPU Buffer 생성
    // ------------------------------------------------------------------------
    if (!mesh_.Initialize(
            device,
            meshData.Vertices,
            meshData.Indices))
    {
        lastErrorMessage_ =
            L"HeightMap Terrain Vertex/Index Buffer 생성에 실패했습니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 4. HeightMap Terrain 전용 Shader 초기화
    // ------------------------------------------------------------------------
    if (!shader_.Initialize(
            device,
            shaderDirectory / L"TerrainVS.hlsl",
            shaderDirectory / L"TerrainPS.hlsl"))
    {
        std::wstringstream stream;
        stream
            << L"HeightMap Terrain Shader 초기화 실패\n\n"
            << L"Shader Directory:\n"
            << shaderDirectory.wstring()
            << L"\n\n"
            << shader_.GetLastErrorMessage();

        lastErrorMessage_ = stream.str();
        return false;
    }

    // ------------------------------------------------------------------------
    // 5. Transform Constant Buffer 생성
    // ------------------------------------------------------------------------
    if (!transformBuffer_.Initialize(device))
    {
        lastErrorMessage_ =
            L"HeightMap Terrain Transform Constant Buffer 생성에 실패했습니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 6. Terrain 전용 Wireframe Rasterizer State 생성
    // ------------------------------------------------------------------------
    if (!CreateWireframeRasterizerState(device))
    {
        lastErrorMessage_ =
            L"HeightMap Terrain Wireframe Rasterizer State 생성에 실패했습니다.";

        return false;
    }

    initialized_ = true;
    return true;
}

void HeightMapTerrain::Render(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection)
{
    if (!initialized_ || !context)
    {
        return;
    }

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> previousRasterizerState;
    context->RSGetState(previousRasterizerState.GetAddressOf());

    context->RSSetState(wireframeRasterizerState_.Get());

    shader_.Bind(context);
    mesh_.Bind(context);

    CBTransform transformData = {};
    XMStoreFloat4x4(
        &transformData.WorldViewProjection,
        XMMatrixTranspose(
            transform_.GetWorldMatrix() *
            view *
            projection));

    transformBuffer_.Update(context, transformData);
    transformBuffer_.BindVS(context, 0);

    mesh_.Draw(context);

    context->RSSetState(previousRasterizerState.Get());
}

bool HeightMapTerrain::IsInitialized() const
{
    return initialized_;
}

const std::wstring& HeightMapTerrain::GetLastErrorMessage() const
{
    return lastErrorMessage_;
}

Transform& HeightMapTerrain::GetTransform()
{
    return transform_;
}

const Transform& HeightMapTerrain::GetTransform() const
{
    return transform_;
}

bool HeightMapTerrain::CreateWireframeRasterizerState(ID3D11Device* device)
{
    D3D11_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
    rasterizerDesc.CullMode = D3D11_CULL_BACK;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthClipEnable = TRUE;

    const HRESULT result =
        device->CreateRasterizerState(
            &rasterizerDesc,
            wireframeRasterizerState_.GetAddressOf());

    return SUCCEEDED(result);
}
