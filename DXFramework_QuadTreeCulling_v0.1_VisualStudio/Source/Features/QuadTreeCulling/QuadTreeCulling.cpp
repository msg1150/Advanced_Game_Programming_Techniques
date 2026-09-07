// ============================================================================
// QuadTreeCulling.cpp
// ============================================================================

#include "Features/QuadTreeCulling/QuadTreeCulling.h"

using namespace DirectX;

void QuadTreeCulling::CollectVisibleRanges(
    const QuadTree& quadTree,
    const Frustum& frustum,
    const XMMATRIX& world,
    bool cullingEnabled,
    std::vector<QuadTreeDrawRange>& outDrawRanges,
    QuadTreeCullingStats& outStats)
{
    outDrawRanges.clear();
    outStats = {};

    outStats.TotalNodes =
        quadTree.GetTotalNodeCount();

    outStats.TotalLeaves =
        quadTree.GetTotalLeafCount();

    outStats.TotalTriangles =
        quadTree.GetTotalIndexCount() /
        3u;

    const QuadTreeNode* root =
        quadTree.GetRoot();

    if (!root)
    {
        return;
    }

    // ------------------------------------------------------------------------
    // Culling OFF
    //
    // 비교 기준을 정직하게 유지하기 위해 전체 Terrain을 Root Range
    // DrawIndexed 1회로 그린다.
    //
    // 따라서 QuadTree Culling의 효과는 주로 Rendered Triangles 감소로 본다.
    // Draw Call은 Culling ON일 때 오히려 여러 Range 때문에 늘 수 있다.
    // ------------------------------------------------------------------------
    if (!cullingEnabled)
    {
        outDrawRanges.push_back(
            {
                root->StartIndex,
                root->IndexCount
            });

        outStats.VisibleLeaves =
            root->LeafCount;

        outStats.DrawCalls = 1u;

        outStats.RenderedTriangles =
            root->IndexCount /
            3u;

        return;
    }

    TraverseNode(
        *root,
        frustum,
        world,
        outDrawRanges,
        outStats);

    outStats.DrawCalls =
        static_cast<std::uint32_t>(
            outDrawRanges.size());
}

void QuadTreeCulling::TraverseNode(
    const QuadTreeNode& node,
    const Frustum& frustum,
    const XMMATRIX& world,
    std::vector<QuadTreeDrawRange>& outDrawRanges,
    QuadTreeCullingStats& stats)
{
    // Node Bounds는 Terrain Local Space에 있으므로
    // 현재 Terrain Transform을 적용한 World AABB로 변환한다.
    BoundingBox worldBounds;

    node.Bounds.Transform(
        worldBounds,
        world);

    const ContainmentType containment =
        frustum.Contains(
            worldBounds);

    // ------------------------------------------------------------------------
    // 완전히 화면 밖
    //
    // Parent가 화면 밖이면 Child 전체를 검사하지 않고 통째로 버린다.
    // 이것이 QuadTree 계층 구조를 사용하는 핵심 이유다.
    // ------------------------------------------------------------------------
    if (containment ==
        DISJOINT)
    {
        ++stats.CulledNodes;

        stats.CulledLeaves +=
            node.LeafCount;

        return;
    }

    // ------------------------------------------------------------------------
    // Node 전체가 Frustum 안
    //
    // 하위 Leaf Index가 연속 배치되어 있으므로
    // Child를 더 검사하지 않고 Parent Range 하나를 Draw한다.
    // ------------------------------------------------------------------------
    if (containment ==
        CONTAINS)
    {
        outDrawRanges.push_back(
            {
                node.StartIndex,
                node.IndexCount
            });

        stats.VisibleLeaves +=
            node.LeafCount;

        stats.RenderedTriangles +=
            node.IndexCount /
            3u;

        return;
    }

    // ------------------------------------------------------------------------
    // 일부만 겹치고 현재 Node가 Leaf라면
    // Leaf 전체를 보수적으로 Draw한다.
    // ------------------------------------------------------------------------
    if (node.IsLeaf())
    {
        outDrawRanges.push_back(
            {
                node.StartIndex,
                node.IndexCount
            });

        ++stats.VisibleLeaves;

        stats.RenderedTriangles +=
            node.IndexCount /
            3u;

        return;
    }

    // INTERSECTS인 Internal Node는 Child까지 내려가 더 세밀하게 검사한다.
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
            outDrawRanges,
            stats);
    }
}
