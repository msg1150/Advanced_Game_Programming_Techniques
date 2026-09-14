// ============================================================================
// QuadTreeLODTerrain.cpp
// ============================================================================

#include "Features/QuadTreeLOD/QuadTreeLODTerrain.h"

using namespace DirectX;

bool QuadTreeLODTerrain::Initialize(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::filesystem::path& shaderDirectory,
    const std::filesystem::path& heightMapImagePath,
    const HeightMapTerrainSettings& terrainSettings,
    const TextureSplatMaterialDesc& materialDesc,
    const QuadTreeLODTerrainSettings& settings)
{
    initialized_ = false;
    lastErrorMessage_.clear();

    if (!device ||
        !context)
    {
        lastErrorMessage_ =
            L"QuadTreeLODTerrain에 전달된 Device 또는 Context가 nullptr입니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 1. 기존 HeightMap Loader 재사용
    // ------------------------------------------------------------------------
    if (!heightMapImage_.Load(
            heightMapImagePath))
    {
        lastErrorMessage_ =
            L"QuadTree LOD HeightMap 로드 실패\n\n" +
            heightMapImage_.GetLastErrorMessage();

        return false;
    }

    // ------------------------------------------------------------------------
    // 2. 기존 HeightMap Terrain Generator 재사용
    //
    // Position / Smooth Normal / UV 생성 방식을 변경하지 않는다.
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
            L"QuadTree LOD Terrain Geometry 생성 실패\n" +
            generatorError;

        return false;
    }

    // ------------------------------------------------------------------------
    // 3. 독립 QuadTree LOD Build
    //
    // 기존 Features/QuadTreeCulling/QuadTree 코드는 손대지 않는다.
    // ------------------------------------------------------------------------
    lodSettings_ =
        settings.LOD;

    std::wstring lodError;

    if (!quadTreeLOD_.Build(
            terrainMeshData.Vertices,
            heightMapImage_.GetWidth(),
            heightMapImage_.GetHeight(),
            lodSettings_,
            lodMeshData_,
            lodError))
    {
        lastErrorMessage_ =
            L"QuadTree LOD 생성 실패\n" +
            lodError;

        return false;
    }

    // ------------------------------------------------------------------------
    // 4. 기존 공용 Mesh
    //
    // Vertex:
    // 원본 HeightMap + Feature 전용 Skirt Vertex
    //
    // Index:
    // Full Resolution + 각 Node LOD Patch
    // ------------------------------------------------------------------------
    if (!mesh_.Initialize(
            device,
            lodMeshData_.Vertices,
            lodMeshData_.Indices))
    {
        lastErrorMessage_ =
            L"QuadTree LOD Terrain GPU Mesh 생성에 실패했습니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 5. 이전 단계의 Height/Slope Texture Splatting Material 그대로 재사용
    // ------------------------------------------------------------------------
    if (!material_.Initialize(
            device,
            context,
            shaderDirectory,
            materialDesc))
    {
        lastErrorMessage_ =
            L"QuadTree LOD Texture Splat Material 초기화 실패\n\n" +
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
            L"QuadTree LOD Transform Constant Buffer 생성에 실패했습니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 7. Feature Local Solid Rasterizer
    // ------------------------------------------------------------------------
    if (!CreateSolidRasterizerState(
            device))
    {
        lastErrorMessage_ =
            L"QuadTree LOD Solid Rasterizer State 생성에 실패했습니다.";

        return false;
    }

    cullingEnabled_ =
        settings.EnableCulling;

    lodEnabled_ =
        settings.EnableLOD;

    drawRanges_.reserve(
        quadTreeLOD_.GetTotalLeafCount());

    initialized_ = true;
    return true;
}

void QuadTreeLODTerrain::Render(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    const XMFLOAT3& cameraWorldPosition)
{
    if (!initialized_ ||
        !context)
    {
        return;
    }

    const XMMATRIX world =
        transform_.GetWorldMatrix();

    // ------------------------------------------------------------------------
    // F1 OFF + F2 OFF
    //
    // 비교 기준을 명확하게 하기 위해 이전 Terrain과 동일한 Full Resolution
    // 전체 Index Range를 DrawIndexed 1회로 그린다.
    // ------------------------------------------------------------------------
    if (!cullingEnabled_ &&
        !lodEnabled_)
    {
        drawRanges_.clear();

        drawRanges_.push_back(
            {
                lodMeshData_.FullResolutionRange.StartIndex,
                lodMeshData_.FullResolutionRange.IndexCount,
                lodMeshData_.FullResolutionRange.SurfaceTriangleCount,
                0u
            });

        BuildFullResolutionOnlyStats();
    }
    else
    {
        if (cullingEnabled_)
        {
            frustum_.Build(
                view,
                projection);
        }

        QuadTreeLODSelector::CollectDrawRanges(
            quadTreeLOD_,
            frustum_,
            world,
            cameraWorldPosition,
            lodSettings_,
            cullingEnabled_,
            lodEnabled_,
            lodMeshData_.FullResolutionRange.SurfaceTriangleCount,
            drawRanges_,
            stats_);
    }

    if (drawRanges_.empty())
    {
        return;
    }

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

    for (const QuadTreeLODDrawRange& range : drawRanges_)
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

void QuadTreeLODTerrain::SetCullingEnabled(
    bool enabled)
{
    cullingEnabled_ =
        enabled;
}

bool QuadTreeLODTerrain::IsCullingEnabled() const
{
    return cullingEnabled_;
}

void QuadTreeLODTerrain::SetLODEnabled(
    bool enabled)
{
    lodEnabled_ =
        enabled;
}

bool QuadTreeLODTerrain::IsLODEnabled() const
{
    return lodEnabled_;
}

const QuadTreeLODStats& QuadTreeLODTerrain::GetStats() const
{
    return stats_;
}

const std::wstring& QuadTreeLODTerrain::GetLastErrorMessage() const
{
    return lastErrorMessage_;
}

Transform& QuadTreeLODTerrain::GetTransform()
{
    return transform_;
}

const Transform& QuadTreeLODTerrain::GetTransform() const
{
    return transform_;
}

bool QuadTreeLODTerrain::CreateSolidRasterizerState(
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

void QuadTreeLODTerrain::BuildFullResolutionOnlyStats()
{
    stats_ = {};

    stats_.TotalNodes =
        quadTreeLOD_.GetTotalNodeCount();

    stats_.TotalLeaves =
        quadTreeLOD_.GetTotalLeafCount();

    stats_.ActiveNodes = 1u;
    stats_.DrawCalls = 1u;

    stats_.FullResolutionSurfaceTriangles =
        lodMeshData_.FullResolutionRange.SurfaceTriangleCount;

    stats_.RenderedSurfaceTriangles =
        lodMeshData_.FullResolutionRange.SurfaceTriangleCount;

    stats_.ActualDrawTriangles =
        lodMeshData_.FullResolutionRange.IndexCount /
        3u;

    stats_.MaxLODLevel =
        quadTreeLOD_.GetMaxDepth();

    // LOD 기능이 OFF인 Full Resolution 비교 경로이므로
    // 가장 세밀한 LOD0로 표시한다.
    stats_.ActiveNodesPerLOD[0] =
        1u;
}
