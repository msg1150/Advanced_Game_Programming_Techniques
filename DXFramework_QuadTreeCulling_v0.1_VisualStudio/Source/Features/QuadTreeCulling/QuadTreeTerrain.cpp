// ============================================================================
// QuadTreeTerrain.cpp
// ============================================================================

#include "Features/QuadTreeCulling/QuadTreeTerrain.h"

using namespace DirectX;

bool QuadTreeTerrain::Initialize(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::filesystem::path& shaderDirectory,
    const std::filesystem::path& heightMapImagePath,
    const HeightMapTerrainSettings& terrainSettings,
    const TextureSplatMaterialDesc& materialDesc,
    const QuadTreeTerrainSettings& quadTreeSettings)
{
    initialized_ = false;
    lastErrorMessage_.clear();

    if (!device ||
        !context)
    {
        lastErrorMessage_ =
            L"QuadTreeTerrain에 전달된 Device 또는 Context가 nullptr입니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 1. 기존 HeightMap 이미지 로드
    // ------------------------------------------------------------------------
    if (!heightMapImage_.Load(
            heightMapImagePath))
    {
        lastErrorMessage_ =
            L"QuadTree Terrain HeightMap 로드 실패\n\n" +
            heightMapImage_.GetLastErrorMessage();

        return false;
    }

    // ------------------------------------------------------------------------
    // 2. 기존 HeightMap Generator로 Vertex / Normal / UV 생성
    //
    // Generator가 만든 기본 Index는 여기서는 사용하지 않는다.
    // QuadTree가 Leaf 순서대로 Index Buffer를 다시 구성한다.
    // ------------------------------------------------------------------------
    HeightMapTerrainMeshData terrainMeshData;
    std::wstring generatorError;

    if (!HeightMapTerrainGenerator::Generate(
            heightMapImage_,
            terrainSettings,
            terrainMeshData,
            generatorError))
    {
        lastErrorMessage_ =
            L"QuadTree Terrain Geometry 생성 실패\n" +
            generatorError;

        return false;
    }

    // ------------------------------------------------------------------------
    // 3. QuadTree 생성 + Index Buffer 재배치
    // ------------------------------------------------------------------------
    std::vector<std::uint32_t> quadTreeIndices;
    std::wstring quadTreeError;

    if (!quadTree_.Build(
            terrainMeshData.Vertices,
            heightMapImage_.GetWidth(),
            heightMapImage_.GetHeight(),
            quadTreeSettings.QuadTree,
            quadTreeIndices,
            quadTreeError))
    {
        lastErrorMessage_ =
            L"QuadTree 생성 실패\n" +
            quadTreeError;

        return false;
    }

    // ------------------------------------------------------------------------
    // 4. 공용 Mesh로 GPU Buffer 생성
    // ------------------------------------------------------------------------
    if (!mesh_.Initialize(
            device,
            terrainMeshData.Vertices,
            quadTreeIndices))
    {
        lastErrorMessage_ =
            L"QuadTree Terrain GPU Mesh 생성에 실패했습니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 5. 기존 Height/Slope Texture Splat Material 재사용
    // ------------------------------------------------------------------------
    if (!material_.Initialize(
            device,
            context,
            shaderDirectory,
            materialDesc))
    {
        lastErrorMessage_ =
            L"QuadTree Terrain Texture Splat Material 초기화 실패\n\n" +
            material_.GetLastErrorMessage();

        return false;
    }

    // ------------------------------------------------------------------------
    // 6. Transform Constant Buffer
    // ------------------------------------------------------------------------
    if (!transformBuffer_.Initialize(
            device))
    {
        lastErrorMessage_ =
            L"QuadTree Terrain Transform Constant Buffer 생성에 실패했습니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 7. Solid Rasterizer
    // ------------------------------------------------------------------------
    if (!CreateSolidRasterizerState(
            device))
    {
        lastErrorMessage_ =
            L"QuadTree Terrain Solid Rasterizer State 생성에 실패했습니다.";

        return false;
    }

    cullingEnabled_ =
        quadTreeSettings.EnableCulling;

    // 최대 Leaf 수 정도를 미리 예약하여
    // 매 Frame vector 재할당을 줄인다.
    visibleRanges_.reserve(
        quadTree_.GetTotalLeafCount());

    initialized_ = true;
    return true;
}

void QuadTreeTerrain::Render(
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
    // 1. 현재 Camera Frustum 생성
    // ------------------------------------------------------------------------
    frustum_.Build(
        view,
        projection);

    const XMMATRIX world =
        transform_.GetWorldMatrix();

    // ------------------------------------------------------------------------
    // 2. QuadTree Frustum Culling
    //
    // 이번 Frame에 Draw할 연속 Index Range 목록과 Debug 통계를 생성한다.
    // ------------------------------------------------------------------------
    QuadTreeCulling::CollectVisibleRanges(
        quadTree_,
        frustum_,
        world,
        cullingEnabled_,
        visibleRanges_,
        cullingStats_);

    if (visibleRanges_.empty())
    {
        return;
    }

    // ------------------------------------------------------------------------
    // 3. 기존 Rasterizer State 저장 후 Solid Terrain Render
    // ------------------------------------------------------------------------
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

    CBTransform transformData = {};

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

    transformBuffer_.BindVS(
        context,
        0u);

    // ------------------------------------------------------------------------
    // 4. Visible Range만 Draw
    //
    // Parent Node가 Frustum에 완전히 포함된 경우
    // 하위 Leaf 여러 개가 하나의 Range로 합쳐져 Draw될 수 있다.
    // ------------------------------------------------------------------------
    for (const QuadTreeDrawRange& range : visibleRanges_)
    {
        mesh_.DrawRange(
            context,
            range.IndexCount,
            range.StartIndex);
    }

    material_.Unbind(
        context);

    context->RSSetState(
        previousRasterizerState.Get());
}

void QuadTreeTerrain::SetCullingEnabled(
    bool enabled)
{
    cullingEnabled_ =
        enabled;
}

bool QuadTreeTerrain::IsCullingEnabled() const
{
    return cullingEnabled_;
}

const QuadTreeCullingStats& QuadTreeTerrain::GetCullingStats() const
{
    return cullingStats_;
}

const std::wstring& QuadTreeTerrain::GetLastErrorMessage() const
{
    return lastErrorMessage_;
}

Transform& QuadTreeTerrain::GetTransform()
{
    return transform_;
}

const Transform& QuadTreeTerrain::GetTransform() const
{
    return transform_;
}

bool QuadTreeTerrain::CreateSolidRasterizerState(
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
