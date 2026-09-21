// ============================================================================
// TriplanarTerrain.cpp
// ============================================================================

#include "Features/TriplanarTerrain/TriplanarTerrain.h"

using namespace DirectX;

bool TriplanarTerrain::Initialize(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::filesystem::path& shaderDirectory,
    const std::filesystem::path& heightMapImagePath,
    const HeightMapTerrainSettings& terrainSettings,
    const TriplanarMaterialDesc& materialDesc,
    const TriplanarTerrainSettings& settings)
{
    initialized_ = false;
    lastErrorMessage_.clear();

    if (!device ||
        !context)
    {
        lastErrorMessage_ =
            L"TriplanarTerrain에 전달된 Device 또는 Context가 nullptr입니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 1. 기존 HeightMap Loader
    // ------------------------------------------------------------------------
    if (!heightMapImage_.Load(
            heightMapImagePath))
    {
        lastErrorMessage_ =
            L"Triplanar Terrain HeightMap 로드 실패\n\n" +
            heightMapImage_.GetLastErrorMessage();

        return false;
    }

    // ------------------------------------------------------------------------
    // 2. 기존 HeightMap Generator
    //
    // Position / Smooth Normal / UV 생성 로직을 변경하지 않는다.
    // Triplanar Shader에서는 UV가 아닌 World Position을 사용하지만,
    // 기존 Vertex Format은 그대로 유지한다.
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
            L"Triplanar Terrain Geometry 생성 실패\n" +
            generatorError;

        return false;
    }

    // ------------------------------------------------------------------------
    // 3. 기존 QuadTreeLOD 재사용
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
            L"Triplanar Terrain QuadTree LOD 생성 실패\n" +
            lodError;

        return false;
    }

    // ------------------------------------------------------------------------
    // 4. 기존 공용 Mesh
    // ------------------------------------------------------------------------
    if (!mesh_.Initialize(
            device,
            lodMeshData_.Vertices,
            lodMeshData_.Indices))
    {
        lastErrorMessage_ =
            L"Triplanar Terrain GPU Mesh 생성에 실패했습니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 5. 새 Triplanar Material
    //
    // Terrain Geometry / LOD는 그대로 두고 Material만 별도 Feature로 교체한다.
    // ------------------------------------------------------------------------
    if (!material_.Initialize(
            device,
            context,
            shaderDirectory,
            materialDesc))
    {
        lastErrorMessage_ =
            L"Triplanar Material 초기화 실패\n\n" +
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
            L"Triplanar Terrain Transform Constant Buffer 생성에 실패했습니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 7. Feature Local Solid Rasterizer
    // ------------------------------------------------------------------------
    if (!CreateSolidRasterizerState(
            device))
    {
        lastErrorMessage_ =
            L"Triplanar Terrain Solid Rasterizer State 생성에 실패했습니다.";

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

void TriplanarTerrain::Render(
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
    // Culling OFF + LOD OFF
    //
    // 이전 단계와 동일한 Full Resolution 비교 경로를 유지한다.
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

void TriplanarTerrain::SetCullingEnabled(
    bool enabled)
{
    cullingEnabled_ =
        enabled;
}

bool TriplanarTerrain::IsCullingEnabled() const
{
    return cullingEnabled_;
}

void TriplanarTerrain::SetLODEnabled(
    bool enabled)
{
    lodEnabled_ =
        enabled;
}

bool TriplanarTerrain::IsLODEnabled() const
{
    return lodEnabled_;
}

const QuadTreeLODStats& TriplanarTerrain::GetStats() const
{
    return stats_;
}

const std::wstring& TriplanarTerrain::GetLastErrorMessage() const
{
    return lastErrorMessage_;
}

Transform& TriplanarTerrain::GetTransform()
{
    return transform_;
}

const Transform& TriplanarTerrain::GetTransform() const
{
    return transform_;
}

bool TriplanarTerrain::CreateSolidRasterizerState(
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

void TriplanarTerrain::BuildFullResolutionOnlyStats()
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

    stats_.ActiveNodesPerLOD[0] =
        1u;
}
