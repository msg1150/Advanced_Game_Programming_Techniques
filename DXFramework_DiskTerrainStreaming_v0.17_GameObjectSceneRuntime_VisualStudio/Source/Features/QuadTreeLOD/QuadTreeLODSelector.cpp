// ============================================================================
// QuadTreeLODSelector.cpp
// ============================================================================

#include "Features/QuadTreeLOD/QuadTreeLODSelector.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

void QuadTreeLODSelector::CollectDrawRanges(
    const QuadTreeLOD& quadTree,
    const Frustum& frustum,
    const XMMATRIX& world,
    const XMFLOAT3& cameraWorldPosition,
    const QuadTreeLODSettings& settings,
    bool cullingEnabled,
    bool lodEnabled,
    std::uint32_t fullResolutionSurfaceTriangles,
    std::vector<QuadTreeLODDrawRange>& outDrawRanges,
    QuadTreeLODStats& outStats)
{
    outDrawRanges.clear();
    outStats = {};

    outStats.TotalNodes =
        quadTree.GetTotalNodeCount();

    outStats.TotalLeaves =
        quadTree.GetTotalLeafCount();

    outStats.MaxLODLevel =
        quadTree.GetMaxDepth();

    outStats.FullResolutionSurfaceTriangles =
        fullResolutionSurfaceTriangles;

    const QuadTreeLODNode* root =
        quadTree.GetRoot();

    if (!root)
    {
        return;
    }

    TraverseNode(
        *root,
        frustum,
        world,
        cameraWorldPosition,
        settings,
        cullingEnabled,
        lodEnabled,
        quadTree.GetMaxDepth(),
        outDrawRanges,
        outStats);

    outStats.DrawCalls =
        static_cast<std::uint32_t>(
            outDrawRanges.size());
}

void QuadTreeLODSelector::TraverseNode(
    const QuadTreeLODNode& node,
    const Frustum& frustum,
    const XMMATRIX& world,
    const XMFLOAT3& cameraWorldPosition,
    const QuadTreeLODSettings& settings,
    bool cullingEnabled,
    bool lodEnabled,
    std::uint32_t maxDepth,
    std::vector<QuadTreeLODDrawRange>& outDrawRanges,
    QuadTreeLODStats& stats)
{
    BoundingBox worldBounds;

    node.Bounds.Transform(
        worldBounds,
        world);

    // ------------------------------------------------------------------------
    // 1. Frustum Culling
    //
    // F1이 OFF면 이 검사를 완전히 건너뛴다.
    // 따라서 Culling과 LOD는 서로 독립적으로 켜고 끌 수 있다.
    // ------------------------------------------------------------------------
    if (cullingEnabled)
    {
        const ContainmentType containment =
            frustum.Contains(
                worldBounds);

        if (containment ==
            DISJOINT)
        {
            ++stats.CulledNodes;

            stats.CulledLeaves +=
                node.LeafCount;

            return;
        }
    }

    // ------------------------------------------------------------------------
    // 2. LOD 선택
    //
    // LOD OFF:
    // 무조건 Leaf까지 내려가서 원본 Grid 해상도를 사용한다.
    //
    // LOD ON:
    // Camera가 충분히 가까운 Node만 Child로 내려간다.
    // 멀리 있는 Node는 현재 Parent Patch에서 멈춘다.
    // ------------------------------------------------------------------------
    if (ShouldSplit(
            node,
            worldBounds,
            cameraWorldPosition,
            settings,
            lodEnabled))
    {
        for (const auto& child : node.Children)
        {
            if (!child)
            {
                continue;
            }

            TraverseNode(
                *child,
                frustum,
                world,
                cameraWorldPosition,
                settings,
                cullingEnabled,
                lodEnabled,
                maxDepth,
                outDrawRanges,
                stats);
        }

        return;
    }

    // ------------------------------------------------------------------------
    // 3. 현재 Node를 실제 Draw 대상으로 선택
    //
    // Leaf Depth가 maxDepth이므로:
    // LODLevel = 0 -> 가장 세밀함
    // 숫자 증가    -> 더 성긴 Parent
    // ------------------------------------------------------------------------
    const std::uint32_t lodLevel =
        maxDepth >= node.Depth
        ? maxDepth - node.Depth
        : 0u;

    outDrawRanges.push_back(
        {
            node.StartIndex,
            node.IndexCount,
            node.SurfaceTriangleCount,
            lodLevel
        });

    ++stats.ActiveNodes;

    stats.RenderedSurfaceTriangles +=
        node.SurfaceTriangleCount;

    stats.ActualDrawTriangles +=
        node.IndexCount /
        3u;

    const std::size_t statIndex =
        std::min<std::size_t>(
            lodLevel,
            kQuadTreeLODTrackedLevelCount - 1u);

    ++stats.ActiveNodesPerLOD[statIndex];
}

float QuadTreeLODSelector::DistanceToBounds(
    const XMFLOAT3& point,
    const BoundingBox& bounds)
{
    // Point와 AABB 사이의 최단 거리.
    //
    // 단순히 Node Center 거리만 사용하는 것보다
    // 큰 Parent Node 근처에서 LOD가 지나치게 늦게 세분화되는 문제를 줄인다.
    const float deltaX =
        std::max(
            std::abs(
                point.x -
                bounds.Center.x) -
            bounds.Extents.x,
            0.0f);

    const float deltaY =
        std::max(
            std::abs(
                point.y -
                bounds.Center.y) -
            bounds.Extents.y,
            0.0f);

    const float deltaZ =
        std::max(
            std::abs(
                point.z -
                bounds.Center.z) -
            bounds.Extents.z,
            0.0f);

    return std::sqrt(
        deltaX * deltaX +
        deltaY * deltaY +
        deltaZ * deltaZ);
}

bool QuadTreeLODSelector::ShouldSplit(
    const QuadTreeLODNode& node,
    const BoundingBox& worldBounds,
    const XMFLOAT3& cameraWorldPosition,
    const QuadTreeLODSettings& settings,
    bool lodEnabled)
{
    if (node.IsLeaf())
    {
        return false;
    }

    // LOD가 꺼져 있으면 모든 Internal Node를 계속 분할해서
    // 최종 Leaf(LOD0)까지 내려간다.
    if (!lodEnabled)
    {
        return true;
    }

    const float distance =
        DistanceToBounds(
            cameraWorldPosition,
            worldBounds);

    // XZ 기준 Node World Size.
    //
    // Terrain은 수평 공간 분할이 핵심이므로 Y 높이는 LOD 거리 기준 크기에서
    // 제외한다. Bounds 자체의 Frustum 판정에는 Y가 정상적으로 포함된다.
    const float nodeWorldSize =
        std::max(
            worldBounds.Extents.x * 2.0f,
            worldBounds.Extents.z * 2.0f);

    const float splitDistance =
        nodeWorldSize *
        settings.SplitDistanceFactor;

    return distance <=
        splitDistance;
}
