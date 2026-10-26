// ============================================================================
// QuadTreeLODSelector.h
// ----------------------------------------------------------------------------
// QuadTreeLOD에서 이번 Frame에 실제로 Draw할 Node를 선택한다.
//
// 역할:
// 1. 선택적으로 Frustum Culling
// 2. Camera와 Node AABB 거리 계산
// 3. 거리 기준 Parent/Child LOD 선택
// 4. Draw Range와 Debug 통계 생성
//
// Renderer / Material / GPU Resource를 소유하지 않는다.
// ============================================================================

#pragma once

#include "Features/QuadTreeLOD/QuadTreeLOD.h"
#include "Graphics/Frustum.h"

#include <DirectXMath.h>

#include <array>
#include <cstdint>
#include <vector>

struct QuadTreeLODDrawRange
{
    std::uint32_t StartIndex = 0u;
    std::uint32_t IndexCount = 0u;

    // Skirt를 제외한 Surface Triangle 수.
    std::uint32_t SurfaceTriangleCount = 0u;

    // LOD0 = 가장 세밀한 Leaf.
    // 숫자가 커질수록 더 성긴 Parent Node 표현.
    std::uint32_t LODLevel = 0u;
};

struct QuadTreeLODStats
{
    std::uint32_t TotalNodes = 0u;
    std::uint32_t TotalLeaves = 0u;

    std::uint32_t ActiveNodes = 0u;
    std::uint32_t CulledNodes = 0u;
    std::uint32_t CulledLeaves = 0u;

    std::uint32_t DrawCalls = 0u;

    // Skirt는 비교 대상에서 제외하고 Terrain Surface만 센다.
    std::uint32_t FullResolutionSurfaceTriangles = 0u;
    std::uint32_t RenderedSurfaceTriangles = 0u;

    // 실제 DrawIndexed에 들어가는 Triangle 수.
    // Skirt가 활성화되어 있으면 Surface보다 값이 클 수 있다.
    std::uint32_t ActualDrawTriangles = 0u;

    std::uint32_t MaxLODLevel = 0u;

    std::array<
        std::uint32_t,
        kQuadTreeLODTrackedLevelCount>
        ActiveNodesPerLOD = {};
};

class QuadTreeLODSelector
{
public:
    static void CollectDrawRanges(
        const QuadTreeLOD& quadTree,
        const Frustum& frustum,
        const DirectX::XMMATRIX& world,
        const DirectX::XMFLOAT3& cameraWorldPosition,
        const QuadTreeLODSettings& settings,
        bool cullingEnabled,
        bool lodEnabled,
        std::uint32_t fullResolutionSurfaceTriangles,
        std::vector<QuadTreeLODDrawRange>& outDrawRanges,
        QuadTreeLODStats& outStats);

private:
    static void TraverseNode(
        const QuadTreeLODNode& node,
        const Frustum& frustum,
        const DirectX::XMMATRIX& world,
        const DirectX::XMFLOAT3& cameraWorldPosition,
        const QuadTreeLODSettings& settings,
        bool cullingEnabled,
        bool lodEnabled,
        std::uint32_t maxDepth,
        std::vector<QuadTreeLODDrawRange>& outDrawRanges,
        QuadTreeLODStats& stats);

    static float DistanceToBounds(
        const DirectX::XMFLOAT3& point,
        const DirectX::BoundingBox& bounds);

    static bool ShouldSplit(
        const QuadTreeLODNode& node,
        const DirectX::BoundingBox& worldBounds,
        const DirectX::XMFLOAT3& cameraWorldPosition,
        const QuadTreeLODSettings& settings,
        bool lodEnabled);
};
