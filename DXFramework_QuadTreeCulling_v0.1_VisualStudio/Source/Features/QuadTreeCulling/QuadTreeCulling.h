// ============================================================================
// QuadTreeCulling.h
// ----------------------------------------------------------------------------
// QuadTree Node와 Camera Frustum을 검사하여
// 이번 Frame에 Draw할 Index Range 목록을 만드는 CPU Culling 클래스.
//
// Renderer나 Shader에 직접 의존하지 않는다.
// ============================================================================

#pragma once

#include "Features/QuadTreeCulling/QuadTree.h"
#include "Graphics/Frustum.h"

#include <DirectXMath.h>

#include <cstdint>
#include <vector>

struct QuadTreeDrawRange
{
    std::uint32_t StartIndex = 0u;
    std::uint32_t IndexCount = 0u;
};

struct QuadTreeCullingStats
{
    std::uint32_t TotalNodes = 0u;
    std::uint32_t TotalLeaves = 0u;

    std::uint32_t VisibleLeaves = 0u;
    std::uint32_t CulledLeaves = 0u;
    std::uint32_t CulledNodes = 0u;

    std::uint32_t DrawCalls = 0u;

    std::uint32_t TotalTriangles = 0u;
    std::uint32_t RenderedTriangles = 0u;
};

class QuadTreeCulling
{
public:
    // Culling ON:
    // Frustum과 Node AABB를 검사해 보이는 Range만 반환한다.
    //
    // Culling OFF:
    // Root 전체 Range 하나를 반환한다.
    static void CollectVisibleRanges(
        const QuadTree& quadTree,
        const Frustum& frustum,
        const DirectX::XMMATRIX& world,
        bool cullingEnabled,
        std::vector<QuadTreeDrawRange>& outDrawRanges,
        QuadTreeCullingStats& outStats);

private:
    static void TraverseNode(
        const QuadTreeNode& node,
        const Frustum& frustum,
        const DirectX::XMMATRIX& world,
        std::vector<QuadTreeDrawRange>& outDrawRanges,
        QuadTreeCullingStats& stats);
};
