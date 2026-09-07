// ============================================================================
// QuadTree.cpp
// ============================================================================

#include "Features/QuadTreeCulling/QuadTree.h"

#include <algorithm>
#include <cfloat>
#include <limits>

using namespace DirectX;

bool QuadTreeNode::IsLeaf() const
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

bool QuadTree::Build(
    const std::vector<Vertex>& vertices,
    std::uint32_t vertexWidth,
    std::uint32_t vertexHeight,
    const QuadTreeSettings& settings,
    std::vector<std::uint32_t>& outIndices,
    std::wstring& outErrorMessage)
{
    root_.reset();

    totalNodeCount_ = 0u;
    totalLeafCount_ = 0u;
    totalIndexCount_ = 0u;

    outIndices.clear();
    outErrorMessage.clear();

    if (vertexWidth < 2u ||
        vertexHeight < 2u)
    {
        outErrorMessage =
            L"QuadTree Terrain Grid는 최소 2x2 Vertex가 필요합니다.";

        return false;
    }

    const std::uint64_t expectedVertexCount =
        static_cast<std::uint64_t>(vertexWidth) *
        static_cast<std::uint64_t>(vertexHeight);

    if (expectedVertexCount !=
        static_cast<std::uint64_t>(vertices.size()))
    {
        outErrorMessage =
            L"QuadTree에 전달된 Vertex Grid 크기와 Vertex 배열 크기가 일치하지 않습니다.";

        return false;
    }

    if (settings.LeafCellSize == 0u)
    {
        outErrorMessage =
            L"QuadTree LeafCellSize는 1 이상이어야 합니다.";

        return false;
    }

    const std::uint32_t totalCellsX =
        vertexWidth - 1u;

    const std::uint32_t totalCellsZ =
        vertexHeight - 1u;

    const std::uint64_t totalIndexCount =
        static_cast<std::uint64_t>(totalCellsX) *
        static_cast<std::uint64_t>(totalCellsZ) *
        6ull;

    if (totalIndexCount >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::uint32_t>::max()))
    {
        outErrorMessage =
            L"QuadTree Terrain Index 개수가 uint32 범위를 초과했습니다.";

        return false;
    }

    outIndices.reserve(
        static_cast<std::size_t>(totalIndexCount));

    root_ =
        BuildNode(
            vertices,
            vertexWidth,
            0u,
            0u,
            totalCellsX,
            totalCellsZ,
            settings.LeafCellSize,
            outIndices);

    if (!root_)
    {
        outErrorMessage =
            L"QuadTree Root Node 생성에 실패했습니다.";

        return false;
    }

    totalIndexCount_ =
        static_cast<std::uint32_t>(
            outIndices.size());

    return true;
}

const QuadTreeNode* QuadTree::GetRoot() const
{
    return root_.get();
}

std::uint32_t QuadTree::GetTotalNodeCount() const
{
    return totalNodeCount_;
}

std::uint32_t QuadTree::GetTotalLeafCount() const
{
    return totalLeafCount_;
}

std::uint32_t QuadTree::GetTotalIndexCount() const
{
    return totalIndexCount_;
}

std::unique_ptr<QuadTreeNode> QuadTree::BuildNode(
    const std::vector<Vertex>& vertices,
    std::uint32_t vertexWidth,
    std::uint32_t cellStartX,
    std::uint32_t cellStartZ,
    std::uint32_t cellCountX,
    std::uint32_t cellCountZ,
    std::uint32_t leafCellSize,
    std::vector<std::uint32_t>& outIndices)
{
    auto node =
        std::make_unique<QuadTreeNode>();

    ++totalNodeCount_;

    node->StartIndex =
        static_cast<std::uint32_t>(
            outIndices.size());

    // ------------------------------------------------------------------------
    // Leaf 종료 조건
    //
    // X/Z 양쪽 Cell 범위가 모두 LeafCellSize 이하가 되면 더 이상 나누지 않는다.
    // ------------------------------------------------------------------------
    if (cellCountX <= leafCellSize &&
        cellCountZ <= leafCellSize)
    {
        node->Bounds =
            CalculateBounds(
                vertices,
                vertexWidth,
                cellStartX,
                cellStartZ,
                cellCountX,
                cellCountZ);

        AppendLeafIndices(
            vertexWidth,
            cellStartX,
            cellStartZ,
            cellCountX,
            cellCountZ,
            outIndices);

        node->IndexCount =
            static_cast<std::uint32_t>(
                outIndices.size()) -
            node->StartIndex;

        node->LeafCount = 1u;

        ++totalLeafCount_;

        return node;
    }

    // ------------------------------------------------------------------------
    // 비정방형 Grid에도 대응하기 위해
    // 각 축이 2개 이상의 Cell을 가질 때만 해당 축을 둘로 나눈다.
    //
    // 일반적인 128x128 Terrain에서는 항상 4개의 Child가 만들어진다.
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
        const std::uint32_t leftCount =
            cellCountX / 2u;

        const std::uint32_t rightCount =
            cellCountX -
            leftCount;

        xStarts[0] =
            cellStartX;

        xStarts[1] =
            cellStartX +
            leftCount;

        xCounts[0] =
            leftCount;

        xCounts[1] =
            rightCount;

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
        const std::uint32_t topCount =
            cellCountZ / 2u;

        const std::uint32_t bottomCount =
            cellCountZ -
            topCount;

        zStarts[0] =
            cellStartZ;

        zStarts[1] =
            cellStartZ +
            topCount;

        zCounts[0] =
            topCount;

        zCounts[1] =
            bottomCount;

        zPartCount = 2u;
    }

    std::uint32_t childSlot = 0u;

    bool hasBounds = false;
    BoundingBox mergedBounds = {};

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
                    vertices,
                    vertexWidth,
                    xStarts[xPart],
                    zStarts[zPart],
                    xCounts[xPart],
                    zCounts[zPart],
                    leafCellSize,
                    outIndices);

            if (!child)
            {
                continue;
            }

            node->LeafCount +=
                child->LeafCount;

            if (!hasBounds)
            {
                mergedBounds =
                    child->Bounds;

                hasBounds = true;
            }
            else
            {
                BoundingBox newBounds;

                BoundingBox::CreateMerged(
                    newBounds,
                    mergedBounds,
                    child->Bounds);

                mergedBounds =
                    newBounds;
            }

            node->Children[childSlot] =
                std::move(child);

            ++childSlot;
        }
    }

    node->Bounds =
        mergedBounds;

    node->IndexCount =
        static_cast<std::uint32_t>(
            outIndices.size()) -
        node->StartIndex;

    return node;
}

BoundingBox QuadTree::CalculateBounds(
    const std::vector<Vertex>& vertices,
    std::uint32_t vertexWidth,
    std::uint32_t cellStartX,
    std::uint32_t cellStartZ,
    std::uint32_t cellCountX,
    std::uint32_t cellCountZ) const
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

    // Cell 영역의 Bounds를 얻으려면
    // 마지막 Cell의 오른쪽/아래 Vertex까지 포함해야 하므로 <= 를 사용한다.
    for (std::uint32_t z = cellStartZ;
         z <= cellStartZ + cellCountZ;
         ++z)
    {
        for (std::uint32_t x = cellStartX;
             x <= cellStartX + cellCountX;
             ++x)
        {
            const Vertex& vertex =
                vertices[
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

    // 완전히 평평한 영역도 유효한 3D Bounds로 검사되도록
    // Y Extent가 지나치게 0이 되는 것을 아주 조금 방어한다.
    bounds.Extents.y =
        std::max(
            bounds.Extents.y,
            0.001f);

    return bounds;
}

void QuadTree::AppendLeafIndices(
    std::uint32_t vertexWidth,
    std::uint32_t cellStartX,
    std::uint32_t cellStartZ,
    std::uint32_t cellCountX,
    std::uint32_t cellCountZ,
    std::vector<std::uint32_t>& outIndices) const
{
    for (std::uint32_t z = cellStartZ;
         z < cellStartZ + cellCountZ;
         ++z)
    {
        for (std::uint32_t x = cellStartX;
             x < cellStartX + cellCountX;
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

            // 기존 HeightMap Terrain과 동일한 +Y Winding.
            outIndices.push_back(
                topLeft);

            outIndices.push_back(
                bottomLeft);

            outIndices.push_back(
                topRight);

            outIndices.push_back(
                topRight);

            outIndices.push_back(
                bottomLeft);

            outIndices.push_back(
                bottomRight);
        }
    }
}
