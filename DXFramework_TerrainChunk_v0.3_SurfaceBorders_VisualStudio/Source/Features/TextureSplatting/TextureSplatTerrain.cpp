// ============================================================================
// TextureSplatTerrain.cpp
// ============================================================================

#include "Features/TextureSplatting/TextureSplatTerrain.h"

using namespace DirectX;

bool TextureSplatTerrain::Initialize(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::filesystem::path& shaderDirectory,
    const std::filesystem::path& heightMapImagePath,
    const HeightMapTerrainSettings& terrainSettings,
    const TextureSplatMaterialDesc& materialDesc)
{
    initialized_ = false;
    lastErrorMessage_.clear();

    if (!device ||
        !context)
    {
        lastErrorMessage_ =
            L"TextureSplatTerrain에 전달된 Device 또는 Context가 nullptr입니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 1. 기존 HeightMap 이미지 로더 재사용
    // ------------------------------------------------------------------------
    if (!heightMapImage_.Load(
            heightMapImagePath))
    {
        lastErrorMessage_ =
            L"Texture Splat Terrain HeightMap 로드 실패\n\n" +
            heightMapImage_.GetLastErrorMessage();

        return false;
    }

    // ------------------------------------------------------------------------
    // 2. 기존 HeightMap Terrain Generator 재사용
    //
    // Generator가 이미 실제 Geometry를 기준으로 Smooth Vertex Normal을
    // 만들어두기 때문에 이번 자동 Splatting에서 그대로 경사도 계산에 쓴다.
    // ------------------------------------------------------------------------
    HeightMapTerrainMeshData meshData;
    std::wstring generatorError;

    if (!HeightMapTerrainGenerator::Generate(
            heightMapImage_,
            terrainSettings,
            meshData,
            generatorError))
    {
        lastErrorMessage_ =
            L"Texture Splat Terrain Mesh 생성 실패\n" +
            generatorError;

        return false;
    }

    // ------------------------------------------------------------------------
    // 3. 기존 Mesh 시스템으로 GPU Buffer 생성
    // ------------------------------------------------------------------------
    if (!mesh_.Initialize(
            device,
            meshData.Vertices,
            meshData.Indices))
    {
        lastErrorMessage_ =
            L"Texture Splat Terrain Vertex/Index Buffer 생성에 실패했습니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 4. Transform Constant Buffer 생성
    // ------------------------------------------------------------------------
    if (!transformBuffer_.Initialize(
            device))
    {
        lastErrorMessage_ =
            L"Texture Splat Terrain Transform Constant Buffer 생성에 실패했습니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 5. 자동 Texture Splat Material 초기화
    // ------------------------------------------------------------------------
    if (!material_.Initialize(
            device,
            context,
            shaderDirectory,
            materialDesc))
    {
        lastErrorMessage_ =
            L"Texture Splat Material 초기화 실패\n\n" +
            material_.GetLastErrorMessage();

        return false;
    }

    // ------------------------------------------------------------------------
    // 6. Texture 확인용 Solid Rasterizer State
    // ------------------------------------------------------------------------
    if (!CreateSolidRasterizerState(
            device))
    {
        lastErrorMessage_ =
            L"Texture Splat Terrain Solid Rasterizer State 생성에 실패했습니다.";

        return false;
    }

    initialized_ = true;
    return true;
}

void TextureSplatTerrain::Render(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection)
{
    if (!initialized_ ||
        !context)
    {
        return;
    }

    // Feature가 변경하는 Rasterizer State를 Render 범위 안에서만 사용한다.
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>
        previousRasterizerState;

    context->RSGetState(
        previousRasterizerState.GetAddressOf());

    context->RSSetState(
        solidRasterizerState_.Get());

    material_.Bind(
        context);

    mesh_.Bind(
        context);

    const XMMATRIX world =
        transform_.GetWorldMatrix();

    CBTransform transformData = {};

    // HLSL Matrix 메모리 배치와 DirectXMath 연산 방식을 맞추기 위해
    // 기존 Framework와 동일하게 Transpose해서 Constant Buffer에 저장한다.
    XMStoreFloat4x4(
        &transformData.WorldViewProjection,
        XMMatrixTranspose(
            world *
            view *
            projection));

    XMStoreFloat4x4(
        &transformData.World,
        XMMatrixTranspose(
            world));

    transformBuffer_.Update(
        context,
        transformData);

    // b0: WVP + World Matrix
    transformBuffer_.BindVS(
        context,
        0u);

    mesh_.Draw(
        context);

    material_.Unbind(
        context);

    context->RSSetState(
        previousRasterizerState.Get());
}

bool TextureSplatTerrain::IsInitialized() const
{
    return initialized_;
}

const std::wstring& TextureSplatTerrain::GetLastErrorMessage() const
{
    return lastErrorMessage_;
}

Transform& TextureSplatTerrain::GetTransform()
{
    return transform_;
}

const Transform& TextureSplatTerrain::GetTransform() const
{
    return transform_;
}

bool TextureSplatTerrain::CreateSolidRasterizerState(
    ID3D11Device* device)
{
    D3D11_RASTERIZER_DESC rasterizerDesc = {};

    rasterizerDesc.FillMode =
        D3D11_FILL_SOLID;

    rasterizerDesc.CullMode =
        D3D11_CULL_BACK;

    rasterizerDesc.FrontCounterClockwise =
        FALSE;

    rasterizerDesc.DepthClipEnable =
        TRUE;

    const HRESULT hr =
        device->CreateRasterizerState(
            &rasterizerDesc,
            solidRasterizerState_.GetAddressOf());

    return SUCCEEDED(hr);
}
