// ============================================================================
// TerrainChunkManager.cpp
// 1. 전체 HeightMap Mesh의 Normal을 단 한 번 생성한다.
// 2. 경계 Vertex가 중복되는 Chunk를 만든다.
// 3. 각 Chunk마다 기존 QuadTreeLOD와 독립 GPU Mesh를 만든다.
// 4. Chunk Frustum 검사 후 내부 QuadTree Culling/LOD를 실행한다.
// 5. TriplanarMaterial은 전체 Chunk가 하나의 인스턴스를 공유한다.
// ============================================================================
#include "Features/TerrainChunk/TerrainChunkManager.h"
#include "Features/HeightMapTerrain/HeightMapImage.h"
#include "Features/HeightMapTerrain/HeightMapTerrainGenerator.h"
#include <DirectXMath.h>
#include <utility>

using namespace DirectX;

bool TerrainChunkManager::Initialize(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::filesystem::path& heightMapPath,
    const HeightMapTerrainSettings& heightSettings,
    const std::filesystem::path& shaderDirectory,
    const TriplanarMaterialDesc& materialDesc,
    const TerrainChunkSettings& settings)
{
    initialized_ = false;
    chunks_.clear();
    stats_ = {};
    lastErrorMessage_.clear();
    if (!device || !context || settings.CellsPerChunk == 0u)
    {
        lastErrorMessage_ = L"Chunk 초기화 인자 오류: Device/Context/CellsPerChunk를 확인하세요.";
        return false;
    }
    settings_ = settings;

    HeightMapImage heightMap;
    if (!heightMap.Load(heightMapPath))
    {
        lastErrorMessage_ = L"Chunk HeightMap 읽기 실패: " + heightMap.GetLastErrorMessage();
        return false;
    }
    HeightMapTerrainMeshData fullMesh;
    std::wstring error;
    if (!HeightMapTerrainGenerator::Generate(heightMap, heightSettings, fullMesh, error))
    {
        lastErrorMessage_ = L"Chunk 전체 Mesh 생성 실패: " + error;
        return false;
    }
    std::vector<TerrainChunkMeshData> cpuChunks;
    if (!TerrainChunkGenerator::Generate(
            fullMesh, heightMap.GetWidth(), heightMap.GetHeight(),
            settings.CellsPerChunk, cpuChunks, error))
    {
        lastErrorMessage_ = L"Chunk 분할 실패: " + error;
        return false;
    }
    if (!material_.Initialize(device, context, shaderDirectory, materialDesc))
    {
        lastErrorMessage_ = L"Chunk Triplanar Material 초기화 실패: " + material_.GetLastErrorMessage();
        return false;
    }

    // Chunk 경계선은 Basic Color Shader를 재사용한다.
    if (!chunkBoundsShader_.Initialize(
            device,
            shaderDirectory.parent_path() / L"Basic" / L"BasicVS.hlsl",
            shaderDirectory.parent_path() / L"Basic" / L"BasicPS.hlsl"))
    {
        lastErrorMessage_ =
            L"Chunk Bounds Debug Shader 초기화 실패: " +
            chunkBoundsShader_.GetLastErrorMessage();
        return false;
    }
    if (!transformBuffer_.Initialize(device) || !CreateRasterizers(device))
    {
        lastErrorMessage_ = L"Chunk Transform ConstantBuffer 또는 Rasterizer 생성에 실패했습니다.";
        return false;
    }
    // 마지막 열/행은 해당 바깥 테두리까지 그려야 전체 Terrain의
    // 네 방향 경계선이 빠짐없이 표시된다.
    // 129x129 Vertex, 32 Cells/Chunk라면 마지막 Chunk Index는 3이다.
    const std::uint32_t lastChunkX =
        (heightMap.GetWidth() - 2u) / settings.CellsPerChunk;

    const std::uint32_t lastChunkZ =
        (heightMap.GetHeight() - 2u) / settings.CellsPerChunk;

    chunks_.reserve(cpuChunks.size());
    for (const TerrainChunkMeshData& cpu : cpuChunks)
    {
        auto chunk = std::make_unique<Chunk>();
        chunk->X = cpu.ChunkX;
        chunk->Z = cpu.ChunkZ;
        if (!chunk->Tree.Build(
                cpu.Vertices, cpu.VertexWidth, cpu.VertexHeight,
                settings.LOD, chunk->MeshData, error))
        {
            lastErrorMessage_ = L"Chunk QuadTree LOD 생성 실패: " + error;
            chunks_.clear();
            return false;
        }
        if (!chunk->GpuMesh.Initialize(device, chunk->MeshData.Vertices, chunk->MeshData.Indices))
        {
            lastErrorMessage_ = L"Chunk GPU Vertex/Index Buffer 생성에 실패했습니다.";
            chunks_.clear();
            return false;
        }

        // 기존 v0.2에서는 root->Bounds 위쪽 Y에 수평 사각형을 생성해
        // 청크마다 서로 다른 높이로 공중에 뜨는 문제가 있었다.
        // 이제 실제 HeightMap Vertex 높이를 그대로 사용한다.
        if (!CreateChunkBoundsMesh(
                device,
                cpu,
                cpu.ChunkX == lastChunkX,
                cpu.ChunkZ == lastChunkZ,
                chunk->BoundsMesh))
        {
            lastErrorMessage_ =
                L"Chunk 표면 경계선 Mesh 생성에 실패했습니다.";
            chunks_.clear();
            return false;
        }

        chunk->Ranges.reserve(chunk->Tree.GetTotalLeafCount());
        stats_.FullResolutionSurfaceTriangles +=
            chunk->MeshData.FullResolutionRange.SurfaceTriangleCount;
        chunks_.push_back(std::move(chunk));
    }
    stats_.TotalChunks = static_cast<std::uint32_t>(chunks_.size());
    initialized_ = true;
    return true;
}

void TerrainChunkManager::Render(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    const XMFLOAT3& cameraPosition)
{
    if (!initialized_ || !context) return;

    // 누적 Full Resolution 수치는 Camera와 관계없이 항상 고정이다.
    const std::uint32_t fullTriangles = stats_.FullResolutionSurfaceTriangles;
    stats_ = {};
    stats_.TotalChunks = static_cast<std::uint32_t>(chunks_.size());
    stats_.FullResolutionSurfaceTriangles = fullTriangles;

    const XMMATRIX world = transform_.GetWorldMatrix();
    if (settings_.EnableCulling) frustum_.Build(view, projection);

    // Triplanar Material / World Transform은 모든 Chunk가 공유한다.
    CBTransform transforms = {};
    XMStoreFloat4x4(&transforms.WorldViewProjection, XMMatrixTranspose(world * view * projection));
    XMStoreFloat4x4(&transforms.World, XMMatrixTranspose(world));
    transformBuffer_.Update(context, transforms);

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> previousRasterizer;
    context->RSGetState(previousRasterizer.GetAddressOf());
    context->RSSetState(rasterizer_.Get());
    material_.Bind(context);
    transformBuffer_.BindVS(context, 0u);

    // --------------------------------------------------------------------
    // 1차 Pass: 실제 Terrain Surface 렌더링
    // --------------------------------------------------------------------
    for (const auto& chunk : chunks_)
    {
        chunk->VisibleThisFrame = false;

        const QuadTreeLODNode* root = chunk->Tree.GetRoot();
        if (!root) continue;
        if (settings_.EnableCulling)
        {
            BoundingBox worldBounds;
            root->Bounds.Transform(worldBounds, world);
            if (frustum_.Contains(worldBounds) == DISJOINT)
            {
                ++stats_.CulledChunks;
                continue;
            }
        }

        chunk->Ranges.clear();
        chunk->FrameStats = {};

        if (!settings_.EnableCulling && !settings_.EnableLOD)
        {
            // 비교 기준: 각 Chunk가 원본 해상도 Mesh를 1회 Draw.
            const auto& full = chunk->MeshData.FullResolutionRange;
            chunk->Ranges.push_back({full.StartIndex, full.IndexCount,
                                     full.SurfaceTriangleCount, 0u});
            chunk->FrameStats.RenderedSurfaceTriangles = full.SurfaceTriangleCount;
            chunk->FrameStats.ActualDrawTriangles = full.IndexCount / 3u;
            chunk->FrameStats.ActiveNodes = 1u;
        }
        else
        {
            QuadTreeLODSelector::CollectDrawRanges(
                chunk->Tree, frustum_, world, cameraPosition, settings_.LOD,
                settings_.EnableCulling, settings_.EnableLOD,
                chunk->MeshData.FullResolutionRange.SurfaceTriangleCount,
                chunk->Ranges, chunk->FrameStats);
        }
        if (chunk->Ranges.empty())
        {
            // Chunk AABB는 교차하더라도 내부 Node가 모두 Cull될 수 있다.
            ++stats_.CulledChunks;
            continue;
        }

        chunk->VisibleThisFrame = true;
        ++stats_.VisibleChunks;
        stats_.ActiveNodes += chunk->FrameStats.ActiveNodes;
        stats_.RenderedSurfaceTriangles += chunk->FrameStats.RenderedSurfaceTriangles;
        stats_.ActualDrawTriangles += chunk->FrameStats.ActualDrawTriangles;

        chunk->GpuMesh.Bind(context);
        for (const QuadTreeLODDrawRange& range : chunk->Ranges)
        {
            chunk->GpuMesh.DrawRange(context, range.IndexCount, range.StartIndex);
            ++stats_.DrawCalls;
        }
    }

    material_.Unbind(context);

    // --------------------------------------------------------------------
    // 2차 Pass: Chunk 표면 경계선 시각화
    //
    // CPU HeightMap의 실제 Chunk Edge Vertex 높이를 따라 선을 그린다.
    // 한 청크의 최대 높이로 만든 공중 사각형이 아니므로 산/계곡을 따라간다.
    // 표면 렌더링이나 Frustum/LOD 계산에는 영향을 주지 않는다.
    // --------------------------------------------------------------------
    if (showChunkBounds_)
    {
        // Mesh::Bind()는 Triangle List로 설정하므로 경계선 이후에
        // 이전 Primitive Topology를 되돌려 다음 Renderer를 오염시키지 않는다.
        D3D11_PRIMITIVE_TOPOLOGY previousTopology =
            D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        context->IAGetPrimitiveTopology(&previousTopology);

        chunkBoundsShader_.Bind(context);
        transformBuffer_.BindVS(context, 0u);
        context->RSSetState(lineRasterizer_.Get());

        for (const auto& chunk : chunks_)
        {
            // 실제 Terrain Draw Range가 없는 Chunk에 경계선을 그리지 않는다.
            if (!chunk->VisibleThisFrame)
            {
                continue;
            }

            chunk->BoundsMesh.Bind(context);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
            chunk->BoundsMesh.Draw(context);
        }

        context->IASetPrimitiveTopology(previousTopology);
    }

    context->RSSetState(previousRasterizer.Get());
}

void TerrainChunkManager::SetCullingEnabled(bool enabled) { settings_.EnableCulling = enabled; }
void TerrainChunkManager::SetLODEnabled(bool enabled) { settings_.EnableLOD = enabled; }
void TerrainChunkManager::SetChunkBoundsVisible(bool visible) { showChunkBounds_ = visible; }
bool TerrainChunkManager::IsCullingEnabled() const { return settings_.EnableCulling; }
bool TerrainChunkManager::IsLODEnabled() const { return settings_.EnableLOD; }
bool TerrainChunkManager::IsChunkBoundsVisible() const { return showChunkBounds_; }
Transform& TerrainChunkManager::GetTransform() { return transform_; }
const TerrainChunkStats& TerrainChunkManager::GetStats() const { return stats_; }
const std::wstring& TerrainChunkManager::GetLastErrorMessage() const { return lastErrorMessage_; }

bool TerrainChunkManager::CreateRasterizers(ID3D11Device* device)
{
    D3D11_RASTERIZER_DESC solidDescription = {};
    solidDescription.FillMode = D3D11_FILL_SOLID;
    solidDescription.CullMode = D3D11_CULL_BACK;
    solidDescription.FrontCounterClockwise = FALSE;
    solidDescription.DepthClipEnable = TRUE;

    if (FAILED(device->CreateRasterizerState(&solidDescription, rasterizer_.GetAddressOf())))
    {
        return false;
    }

    D3D11_RASTERIZER_DESC lineDescription = {};
    lineDescription.FillMode = D3D11_FILL_SOLID;
    lineDescription.CullMode = D3D11_CULL_NONE;
    lineDescription.FrontCounterClockwise = FALSE;
    lineDescription.DepthClipEnable = TRUE;
    lineDescription.AntialiasedLineEnable = TRUE;

    // Line과 Terrain 표면이 거의 같은 Depth일 때 생기는 깜빡임을 줄인다.
    // 지나친 Bias는 선이 실제 지형에서 떨어져 보일 수 있으므로 작게 지정한다.
    lineDescription.DepthBias = -256;
    lineDescription.SlopeScaledDepthBias = -0.25f;
    lineDescription.DepthBiasClamp = 0.0f;

    return SUCCEEDED(device->CreateRasterizerState(&lineDescription, lineRasterizer_.GetAddressOf()));
}

bool TerrainChunkManager::CreateChunkBoundsMesh(
    ID3D11Device* device,
    const TerrainChunkMeshData& cpuChunk,
    bool drawLastXEdge,
    bool drawLastZEdge,
    Mesh& outMesh)
{
    // ------------------------------------------------------------------------
    // Surface Border 생성
    //
    // 기존: Node AABB의 MaxY를 모든 네 꼭짓점 높이로 지정했다.
    //       산과 평지가 섞여 있으면 청크마다 경계 사각형이 공중에 뜬다.
    //
    // 변경: 전체 HeightMap Generator가 생성한 진짜 경계 Vertex를 이용한다.
    //       인접 Chunk의 동일한 경계 위치/높이가 정확히 일치한다.
    // ------------------------------------------------------------------------
    if (!device ||
        cpuChunk.VertexWidth < 2u ||
        cpuChunk.VertexHeight < 2u ||
        cpuChunk.Vertices.size() !=
            static_cast<std::size_t>(cpuChunk.VertexWidth) *
            static_cast<std::size_t>(cpuChunk.VertexHeight))
    {
        return false;
    }

    const bool even =
        ((cpuChunk.ChunkX + cpuChunk.ChunkZ) % 2u) == 0u;

    const XMFLOAT4 color =
        even
        ? XMFLOAT4(1.0f, 0.55f, 0.15f, 1.0f)
        : XMFLOAT4(0.15f, 0.85f, 1.0f, 1.0f);

    // 원본 지형의 경계 높이만 살짝 올려 Z-Fighting을 줄인다.
    // 청크의 전체 최고점으로 선을 수평 배치하지 않는다.
    constexpr float kSurfaceOffset = 0.025f;

    std::vector<Vertex> lineVertices;
    std::vector<std::uint32_t> lineIndices;

    // 북쪽/서쪽은 모든 Chunk가 소유한다.
    // 오른쪽/아래쪽은 마지막 열/행 Chunk만 소유한다.
    // 따라서 내부 공유 Edge가 중복 Draw되지 않아 두 색이 겹쳐 깜빡이지 않는다.
    const std::size_t maximumSegments =
        2u * static_cast<std::size_t>(cpuChunk.CellsX + cpuChunk.CellsZ);
    lineVertices.reserve(maximumSegments * 2u);
    lineIndices.reserve(maximumSegments * 2u);

    const auto appendSegment =
        [&](std::uint32_t x0, std::uint32_t z0,
            std::uint32_t x1, std::uint32_t z1)
    {
        const std::size_t width =
            static_cast<std::size_t>(cpuChunk.VertexWidth);

        Vertex first =
            cpuChunk.Vertices[static_cast<std::size_t>(z0) * width + x0];
        Vertex second =
            cpuChunk.Vertices[static_cast<std::size_t>(z1) * width + x1];

        // 복제된 두 Vertex는 HeightMap의 표면을 정확히 따라간다.
        first.Position.y += kSurfaceOffset;
        second.Position.y += kSurfaceOffset;
        first.Color = color;
        second.Color = color;

        const std::uint32_t startIndex =
            static_cast<std::uint32_t>(lineVertices.size());

        lineVertices.push_back(first);
        lineVertices.push_back(second);
        lineIndices.push_back(startIndex);
        lineIndices.push_back(startIndex + 1u);
    };

    const std::uint32_t maxX = cpuChunk.VertexWidth - 1u;
    const std::uint32_t maxZ = cpuChunk.VertexHeight - 1u;

    // 북쪽 Edge: Z = 0
    for (std::uint32_t x = 0u; x < maxX; ++x)
    {
        appendSegment(x, 0u, x + 1u, 0u);
    }

    // 서쪽 Edge: X = 0
    for (std::uint32_t z = 0u; z < maxZ; ++z)
    {
        appendSegment(0u, z, 0u, z + 1u);
    }

    // 전체 Terrain의 동쪽 외곽은 마지막 열 Chunk만 그린다.
    if (drawLastXEdge)
    {
        for (std::uint32_t z = 0u; z < maxZ; ++z)
        {
            appendSegment(maxX, z, maxX, z + 1u);
        }
    }

    // 전체 Terrain의 남쪽 외곽은 마지막 행 Chunk만 그린다.
    if (drawLastZEdge)
    {
        for (std::uint32_t x = 0u; x < maxX; ++x)
        {
            appendSegment(x, maxZ, x + 1u, maxZ);
        }
    }

    // Mesh 클래스와 Basic Shader를 그대로 사용한다.
    // Renderer의 기존 Terrain Geometry, QuadTree LOD, Material 코드는 불변.
    return outMesh.Initialize(device, lineVertices, lineIndices);
}
