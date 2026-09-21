// ============================================================================
// QuadTreeLOD.cpp
// ============================================================================

#include "Features/QuadTreeLOD/QuadTreeLOD.h"

#include <algorithm>
#include <cfloat>
#include <limits>

using namespace DirectX;

bool QuadTreeLODNode::IsLeaf() const
{
    for (const auto& child : Children)
    {
        if (child)
        {
            return false;
        }
    }

    return true;
}

bool QuadTreeLOD::Build(
    const std::vector<Vertex>& sourceVertices,
    std::uint32_t vertexWidth,
    std::uint32_t vertexHeight,
    const QuadTreeLODSettings& settings,
    QuadTreeLODMeshData& outMeshData,
    std::wstring& outErrorMessage)
{
    root_.reset();

    totalNodeCount_ = 0u;
    totalLeafCount_ = 0u;
    maxDepth_ = 0u;

    outMeshData = {};
    outErrorMessage.clear();

    if (vertexWidth < 2u ||
        vertexHeight < 2u)
    {
        outErrorMessage =
            L"QuadTree LOD Terrain Grid는 최소 2x2 Vertex가 필요합니다.";

        return false;
    }

    const std::uint64_t expectedVertexCount =
        static_cast<std::uint64_t>(vertexWidth) *
        static_cast<std::uint64_t>(vertexHeight);

    if (expectedVertexCount !=
        static_cast<std::uint64_t>(sourceVertices.size()))
    {
        outErrorMessage =
            L"Vertex Grid 크기와 Vertex 배열 크기가 일치하지 않습니다.";

        return false;
    }

    if (settings.LeafCellSize == 0u)
    {
        outErrorMessage =
            L"LeafCellSize는 1 이상이어야 합니다.";

        return false;
    }

    if (settings.SplitDistanceFactor <= 0.0f)
    {
        outErrorMessage =
            L"SplitDistanceFactor는 0보다 커야 합니다.";

        return false;
    }

    if (settings.SkirtDepth < 0.0f)
    {
        outErrorMessage =
            L"SkirtDepth는 0 이상이어야 합니다.";

        return false;
    }

    const std::uint64_t fullIndexCount =
        static_cast<std::uint64_t>(vertexWidth - 1u) *
        static_cast<std::uint64_t>(vertexHeight - 1u) *
        6ull;

    if (fullIndexCount >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::uint32_t>::max()))
    {
        outErrorMessage =
            L"Terrain Index 수가 uint32 범위를 초과했습니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 원본 HeightMap Vertex는 그대로 유지한다.
    //
    // Skirt에 필요한 Vertex만 뒤에 추가한다.
    // 따라서 기존 Terrain Generator의 Position / Normal / UV 계산을
    // 전혀 수정하지 않는다.
    // ------------------------------------------------------------------------
    outMeshData.Vertices =
        sourceVertices;

    // LOD와 Culling을 둘 다 껐을 때 이전 단계와 동일하게
    // 전체 Terrain을 한 번에 그릴 수 있도록 Full Resolution Range를 먼저 만든다.
    AppendFullResolutionIndices(
        vertexWidth,
        vertexHeight,
        outMeshData);

    const std::uint32_t totalCellsX =
        vertexWidth - 1u;

    const std::uint32_t totalCellsZ =
        vertexHeight - 1u;

    root_ =
        BuildNode(
            sourceVertices,
            vertexWidth,
            0u,
            0u,
            totalCellsX,
            totalCellsZ,
            0u,
            settings,
            outMeshData);

    if (!root_)
    {
        outErrorMessage =
            L"QuadTree LOD Root Node 생성에 실패했습니다.";

        return false;
    }

    return true;
}

const QuadTreeLODNode* QuadTreeLOD::GetRoot() const
{
    return root_.get();
}

std::uint32_t QuadTreeLOD::GetTotalNodeCount() const
{
    return totalNodeCount_;
}

std::uint32_t QuadTreeLOD::GetTotalLeafCount() const
{
    return totalLeafCount_;
}

std::uint32_t QuadTreeLOD::GetMaxDepth() const
{
    return maxDepth_;
}

std::unique_ptr<QuadTreeLODNode> QuadTreeLOD::BuildNode(
    const std::vector<Vertex>& sourceVertices,
    std::uint32_t vertexWidth,
    std::uint32_t cellStartX,
    std::uint32_t cellStartZ,
    std::uint32_t cellCountX,
    std::uint32_t cellCountZ,
    std::uint32_t depth,
    const QuadTreeLODSettings& settings,
    QuadTreeLODMeshData& meshData)
{
    auto node =
        std::make_unique<QuadTreeLODNode>();

    ++totalNodeCount_;

    maxDepth_ =
        std::max(
            maxDepth_,
            depth);

    node->Depth =
        depth;

    // Node 자신을 렌더링할 수 있는 LOD Patch를 먼저 만든다.
    //
    // Root는 전체 Terrain을 성긴 Patch로,
    // Child로 내려갈수록 같은 Patch 해상도가 더 작은 공간을 담당하므로
    // 실제 World Detail이 점점 높아진다.
    AppendNodePatch(
        sourceVertices,
        vertexWidth,
        cellStartX,
        cellStartZ,
        cellCountX,
        cellCountZ,
        settings,
        *node,
        meshData);

    node->Bounds =
        CalculateBounds(
            sourceVertices,
            vertexWidth,
            cellStartX,
            cellStartZ,
            cellCountX,
            cellCountZ,
            settings.SkirtDepth);

    // ------------------------------------------------------------------------
    // Leaf 종료 조건
    // ------------------------------------------------------------------------
    if (cellCountX <= settings.LeafCellSize &&
        cellCountZ <= settings.LeafCellSize)
    {
        node->LeafCount = 1u;

        ++totalLeafCount_;

        return node;
    }

    // ------------------------------------------------------------------------
    // 기존 QuadTree Culling 단계와 동일한 분할 규칙을 독립적으로 사용한다.
    //
    // 비정방형 Grid에서도 한 축만 더 나눌 수 있도록 각 축을 따로 계산한다.
    // ------------------------------------------------------------------------
    std::array<std::uint32_t, 2> xStarts =
    {
        cellStartX,
        cellStartX
    };

    std::array<std::uint32_t, 2> xCounts =
    {
        cellCountX,
        0u
    };

    std::uint32_t xPartCount = 1u;

    if (cellCountX > 1u)
    {
        const std::uint32_t firstCount =
            cellCountX / 2u;

        const std::uint32_t secondCount =
            cellCountX -
            firstCount;

        xStarts[0] =
            cellStartX;

        xStarts[1] =
            cellStartX +
            firstCount;

        xCounts[0] =
            firstCount;

        xCounts[1] =
            secondCount;

        xPartCount = 2u;
    }

    std::array<std::uint32_t, 2> zStarts =
    {
        cellStartZ,
        cellStartZ
    };

    std::array<std::uint32_t, 2> zCounts =
    {
        cellCountZ,
        0u
    };

    std::uint32_t zPartCount = 1u;

    if (cellCountZ > 1u)
    {
        const std::uint32_t firstCount =
            cellCountZ / 2u;

        const std::uint32_t secondCount =
            cellCountZ -
            firstCount;

        zStarts[0] =
            cellStartZ;

        zStarts[1] =
            cellStartZ +
            firstCount;

        zCounts[0] =
            firstCount;

        zCounts[1] =
            secondCount;

        zPartCount = 2u;
    }

    std::uint32_t childSlot = 0u;

    for (std::uint32_t zPart = 0u;
         zPart < zPartCount;
         ++zPart)
    {
        for (std::uint32_t xPart = 0u;
             xPart < xPartCount;
             ++xPart)
        {
            if (childSlot >=
                node->Children.size())
            {
                break;
            }

            auto child =
                BuildNode(
                    sourceVertices,
                    vertexWidth,
                    xStarts[xPart],
                    zStarts[zPart],
                    xCounts[xPart],
                    zCounts[zPart],
                    depth + 1u,
                    settings,
                    meshData);

            if (child)
            {
                node->LeafCount +=
                    child->LeafCount;

                node->Children[childSlot] =
                    std::move(child);
            }

            ++childSlot;
        }
    }

    return node;
}

BoundingBox QuadTreeLOD::CalculateBounds(
    const std::vector<Vertex>& sourceVertices,
    std::uint32_t vertexWidth,
    std::uint32_t cellStartX,
    std::uint32_t cellStartZ,
    std::uint32_t cellCountX,
    std::uint32_t cellCountZ,
    float skirtDepth) const
{
    XMFLOAT3 minimum =
    {
        FLT_MAX,
        FLT_MAX,
        FLT_MAX
    };

    XMFLOAT3 maximum =
    {
        -FLT_MAX,
        -FLT_MAX,
        -FLT_MAX
    };

    for (std::uint32_t z = cellStartZ;
         z <= cellStartZ + cellCountZ;
         ++z)
    {
        for (std::uint32_t x = cellStartX;
             x <= cellStartX + cellCountX;
             ++x)
        {
            const Vertex& vertex =
                sourceVertices[
                    static_cast<std::size_t>(z) *
                    static_cast<std::size_t>(vertexWidth) +
                    static_cast<std::size_t>(x)];

            minimum.x =
                std::min(
                    minimum.x,
                    vertex.Position.x);

            minimum.y =
                std::min(
                    minimum.y,
                    vertex.Position.y);

            minimum.z =
                std::min(
                    minimum.z,
                    vertex.Position.z);

            maximum.x =
                std::max(
                    maximum.x,
                    vertex.Position.x);

            maximum.y =
                std::max(
                    maximum.y,
                    vertex.Position.y);

            maximum.z =
                std::max(
                    maximum.z,
                    vertex.Position.z);
        }
    }

    // Skirt Vertex는 원본 Terrain보다 아래로 내려가므로
    // Frustum Culling Bounds에서도 해당 범위를 포함한다.
    minimum.y -=
        skirtDepth;

    BoundingBox bounds;

    bounds.Center =
    {
        (minimum.x + maximum.x) * 0.5f,
        (minimum.y + maximum.y) * 0.5f,
        (minimum.z + maximum.z) * 0.5f
    };

    bounds.Extents =
    {
        (maximum.x - minimum.x) * 0.5f,
        (maximum.y - minimum.y) * 0.5f,
        (maximum.z - minimum.z) * 0.5f
    };

    bounds.Extents.y =
        std::max(
            bounds.Extents.y,
            0.001f);

    return bounds;
}

void QuadTreeLOD::AppendFullResolutionIndices(
    std::uint32_t vertexWidth,
    std::uint32_t vertexHeight,
    QuadTreeLODMeshData& meshData) const
{
    meshData.FullResolutionRange.StartIndex =
        static_cast<std::uint32_t>(
            meshData.Indices.size());

    for (std::uint32_t z = 0u;
         z < vertexHeight - 1u;
         ++z)
    {
        for (std::uint32_t x = 0u;
             x < vertexWidth - 1u;
             ++x)
        {
            const std::uint32_t topLeft =
                z * vertexWidth +
                x;

            const std::uint32_t topRight =
                topLeft + 1u;

            const std::uint32_t bottomLeft =
                (z + 1u) *
                vertexWidth +
                x;

            const std::uint32_t bottomRight =
                bottomLeft + 1u;

            // 기존 HeightMap Terrain의 +Y Winding을 그대로 유지한다.
            meshData.Indices.push_back(
                topLeft);

            meshData.Indices.push_back(
                bottomLeft);

            meshData.Indices.push_back(
                topRight);

            meshData.Indices.push_back(
                topRight);

            meshData.Indices.push_back(
                bottomLeft);

            meshData.Indices.push_back(
                bottomRight);
        }
    }

    meshData.FullResolutionRange.IndexCount =
        static_cast<std::uint32_t>(
            meshData.Indices.size()) -
        meshData.FullResolutionRange.StartIndex;

    meshData.FullResolutionRange.SurfaceTriangleCount =
        meshData.FullResolutionRange.IndexCount /
        3u;
}

void QuadTreeLOD::AppendNodePatch(
    const std::vector<Vertex>& sourceVertices,
    std::uint32_t vertexWidth,
    std::uint32_t cellStartX,
    std::uint32_t cellStartZ,
    std::uint32_t cellCountX,
    std::uint32_t cellCountZ,
    const QuadTreeLODSettings& settings,
    QuadTreeLODNode& node,
    QuadTreeLODMeshData& meshData) const
{
    node.StartIndex =
        static_cast<std::uint32_t>(
            meshData.Indices.size());

    // ------------------------------------------------------------------------
    // 모든 Node를 최대 LeafCellSize 세그먼트 정도로 표현한다.
    //
    // 예: LeafCellSize = 8
    //
    // Root 128 Cell 영역
    // -> 8x8 Patch로 표현
    //
    // Child 64 Cell 영역
    // -> 동일한 8x8 Patch
    //
    // Leaf 8 Cell 영역
    // -> 8x8 Patch = 원본 Full Resolution
    //
    // 결과적으로 Child로 내려갈수록 World 단위 Detail이 증가한다.
    // ------------------------------------------------------------------------
    const auto xSamples =
        BuildAxisSamples(
            cellStartX,
            cellCountX,
            settings.LeafCellSize);

    const auto zSamples =
        BuildAxisSamples(
            cellStartZ,
            cellCountZ,
            settings.LeafCellSize);

    if (xSamples.size() < 2u ||
        zSamples.size() < 2u)
    {
        return;
    }

    for (std::size_t zIndex = 0u;
         zIndex + 1u < zSamples.size();
         ++zIndex)
    {
        for (std::size_t xIndex = 0u;
             xIndex + 1u < xSamples.size();
             ++xIndex)
        {
            const std::uint32_t x0 =
                xSamples[xIndex];

            const std::uint32_t x1 =
                xSamples[xIndex + 1u];

            const std::uint32_t z0 =
                zSamples[zIndex];

            const std::uint32_t z1 =
                zSamples[zIndex + 1u];

            const std::uint32_t topLeft =
                z0 * vertexWidth +
                x0;

            const std::uint32_t topRight =
                z0 * vertexWidth +
                x1;

            const std::uint32_t bottomLeft =
                z1 * vertexWidth +
                x0;

            const std::uint32_t bottomRight =
                z1 * vertexWidth +
                x1;

            meshData.Indices.push_back(
                topLeft);

            meshData.Indices.push_back(
                bottomLeft);

            meshData.Indices.push_back(
                topRight);

            meshData.Indices.push_back(
                topRight);

            meshData.Indices.push_back(
                bottomLeft);

            meshData.Indices.push_back(
                bottomRight);

            node.SurfaceTriangleCount +=
                2u;
        }
    }

    // ------------------------------------------------------------------------
    // LOD Crack 방어: Skirt
    //
    // 서로 이웃한 Node가 서로 다른 LOD를 사용하면 경계 Vertex 개수가 달라져
    // 틈이 생길 수 있다.
    //
    // 이번 단계에서는 기존 HeightMap Vertex를 수정하거나 Neighbor Stitch
    // Index를 복잡하게 추가하지 않고, Feature 내부에서만 사용할 수 있는
    // Skirt를 외곽 아래쪽으로 생성해 틈을 가린다.
    // ------------------------------------------------------------------------
    if (settings.SkirtDepth > 0.0f)
    {
        std::vector<std::uint32_t> northEdge;
        std::vector<std::uint32_t> southEdge;
        std::vector<std::uint32_t> westEdge;
        std::vector<std::uint32_t> eastEdge;

        northEdge.reserve(
            xSamples.size());

        southEdge.reserve(
            xSamples.size());

        westEdge.reserve(
            zSamples.size());

        eastEdge.reserve(
            zSamples.size());

        const std::uint32_t northZ =
            zSamples.front();

        const std::uint32_t southZ =
            zSamples.back();

        for (const std::uint32_t x : xSamples)
        {
            northEdge.push_back(
                northZ * vertexWidth +
                x);

            southEdge.push_back(
                southZ * vertexWidth +
                x);
        }

        const std::uint32_t westX =
            xSamples.front();

        const std::uint32_t eastX =
            xSamples.back();

        for (const std::uint32_t z : zSamples)
        {
            westEdge.push_back(
                z * vertexWidth +
                westX);

            eastEdge.push_back(
                z * vertexWidth +
                eastX);
        }

        AppendSkirtEdge(
            northEdge,
            settings.SkirtDepth,
            meshData);

        AppendSkirtEdge(
            southEdge,
            settings.SkirtDepth,
            meshData);

        AppendSkirtEdge(
            westEdge,
            settings.SkirtDepth,
            meshData);

        AppendSkirtEdge(
            eastEdge,
            settings.SkirtDepth,
            meshData);
    }

    node.IndexCount =
        static_cast<std::uint32_t>(
            meshData.Indices.size()) -
        node.StartIndex;
}

std::vector<std::uint32_t> QuadTreeLOD::BuildAxisSamples(
    std::uint32_t start,
    std::uint32_t cellCount,
    std::uint32_t maxSegments) const
{
    std::vector<std::uint32_t> samples;

    if (cellCount == 0u ||
        maxSegments == 0u)
    {
        return samples;
    }

    const std::uint32_t segmentCount =
        std::min(
            cellCount,
            maxSegments);

    samples.reserve(
        static_cast<std::size_t>(segmentCount) +
        1u);

    for (std::uint32_t index = 0u;
         index <= segmentCount;
         ++index)
    {
        // 정수 Grid에서 시작/끝을 정확히 포함하면서
        // 중간 Sample을 최대한 균등하게 배치한다.
        const std::uint64_t numerator =
            static_cast<std::uint64_t>(index) *
            static_cast<std::uint64_t>(cellCount);

        const std::uint32_t coordinate =
            start +
            static_cast<std::uint32_t>(
                numerator /
                segmentCount);

        if (samples.empty() ||
            samples.back() != coordinate)
        {
            samples.push_back(
                coordinate);
        }
    }

    const std::uint32_t expectedEnd =
        start +
        cellCount;

    if (samples.empty() ||
        samples.back() != expectedEnd)
    {
        samples.push_back(
            expectedEnd);
    }

    return samples;
}

void QuadTreeLOD::AppendSkirtEdge(
    const std::vector<std::uint32_t>& topVertexIndices,
    float skirtDepth,
    QuadTreeLODMeshData& meshData) const
{
    if (topVertexIndices.size() < 2u)
    {
        return;
    }

    std::vector<std::uint32_t> bottomVertexIndices;

    bottomVertexIndices.reserve(
        topVertexIndices.size());

    for (const std::uint32_t topIndex : topVertexIndices)
    {
        if (topIndex >=
            meshData.Vertices.size())
        {
            return;
        }

        Vertex skirtVertex =
            meshData.Vertices[topIndex];

        skirtVertex.Position.y -=
            skirtDepth;

        const std::uint32_t bottomIndex =
            static_cast<std::uint32_t>(
                meshData.Vertices.size());

        meshData.Vertices.push_back(
            skirtVertex);

        bottomVertexIndices.push_back(
            bottomIndex);
    }

    for (std::size_t index = 0u;
         index + 1u < topVertexIndices.size();
         ++index)
    {
        const std::uint32_t topA =
            topVertexIndices[index];

        const std::uint32_t topB =
            topVertexIndices[index + 1u];

        const std::uint32_t bottomA =
            bottomVertexIndices[index];

        const std::uint32_t bottomB =
            bottomVertexIndices[index + 1u];

        // Skirt가 내부/외부 어느 방향에서 보이더라도 Crack을 가릴 수 있도록
        // 같은 Quad를 양쪽 Winding으로 기록한다.
        //
        // Front side.
        meshData.Indices.push_back(topA);
        meshData.Indices.push_back(bottomA);
        meshData.Indices.push_back(topB);

        meshData.Indices.push_back(topB);
        meshData.Indices.push_back(bottomA);
        meshData.Indices.push_back(bottomB);

        // Back side.
        meshData.Indices.push_back(topA);
        meshData.Indices.push_back(topB);
        meshData.Indices.push_back(bottomA);

        meshData.Indices.push_back(topB);
        meshData.Indices.push_back(bottomB);
        meshData.Indices.push_back(bottomA);
    }
}
