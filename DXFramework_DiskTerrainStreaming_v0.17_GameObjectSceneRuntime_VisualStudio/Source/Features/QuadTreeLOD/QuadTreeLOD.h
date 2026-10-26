// ============================================================================
// QuadTreeLOD.h
// ----------------------------------------------------------------------------
// Terrain 전용 QuadTree LOD 자료구조.
//
// 기존 Features/QuadTreeCulling/QuadTree.*는 전혀 수정하지 않는다.
// 이번 단계의 LOD 기능을 제거할 때 이 폴더만 제거할 수 있도록
// 독립된 Tree / Index 생성 로직으로 분리했다.
//
// 핵심 방식:
// - Terrain 전체를 QuadTree로 재귀 분할한다.
// - 모든 Node는 자기 영역을 약 LeafCellSize x LeafCellSize 정도의
//   Patch 해상도로 표현하는 Index Range를 가진다.
// - Root는 가장 성긴 표현, Leaf는 원본 해상도 표현이 된다.
// - Runtime에는 Camera 거리 기준으로 Parent에서 멈추거나 Child로 내려간다.
// - 서로 다른 LOD 경계의 틈은 Node별 Skirt Geometry로 가린다.
// ============================================================================

#pragma once

#include "Graphics/Mesh.h"

#include <DirectXCollision.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// 화면 Debug 통계에서 개별 LOD를 추적할 최대 개수.
// 현재 129x129 HeightMap / LeafCellSize 8 기준 최대 LOD는 4이다.
constexpr std::size_t kQuadTreeLODTrackedLevelCount = 8u;

struct QuadTreeLODSettings
{
    // 최종 Leaf가 한 축에서 담당할 최대 Terrain Cell 수.
    //
    // 128x128 Cell Terrain에서 8을 사용하면:
    // 128 -> 64 -> 32 -> 16 -> 8
    // 총 5단계(LOD0~LOD4)를 사용할 수 있다.
    std::uint32_t LeafCellSize = 8u;

    // Node를 더 세분화할지 결정하는 거리 계수.
    //
    // SplitDistance = NodeWorldSize * SplitDistanceFactor
    //
    // 값이 클수록 더 먼 거리에서도 Child로 내려가므로
    // 높은 Detail이 오래 유지된다.
    float SplitDistanceFactor = 3.0f;

    // 서로 다른 LOD Patch의 경계에서 생길 수 있는 Crack을 가리기 위해
    // Patch 외곽 Vertex를 아래로 내리는 깊이.
    float SkirtDepth = 0.18f;
};

struct QuadTreeLODNode
{
    // Local Space 기준 AABB.
    // SkirtDepth까지 포함하도록 Y Min을 아래로 확장한다.
    DirectX::BoundingBox Bounds;

    // 이 Node의 LOD Patch가 Index Buffer에서 차지하는 범위.
    std::uint32_t StartIndex = 0u;
    std::uint32_t IndexCount = 0u;

    // Skirt를 제외한 실제 Terrain Surface Triangle 수.
    std::uint32_t SurfaceTriangleCount = 0u;

    // Root = 0, 아래로 내려갈수록 증가한다.
    std::uint32_t Depth = 0u;

    // 이 Node 아래에 존재하는 최종 Leaf 개수.
    std::uint32_t LeafCount = 0u;

    std::array<std::unique_ptr<QuadTreeLODNode>, 4> Children;

    bool IsLeaf() const;
};

struct QuadTreeLODFullResolutionRange
{
    // LOD / Culling을 둘 다 끈 경우 기존 Terrain처럼
    // 전체 Grid를 한 번의 DrawIndexed로 그리기 위한 Range.
    std::uint32_t StartIndex = 0u;
    std::uint32_t IndexCount = 0u;
    std::uint32_t SurfaceTriangleCount = 0u;
};

struct QuadTreeLODMeshData
{
    // 원본 HeightMap Vertex 뒤에 Node Skirt Vertex가 추가된다.
    std::vector<Vertex> Vertices;

    // 1) 전체 Full Resolution Range
    // 2) 각 QuadTree Node의 LOD Patch Range
    // 순으로 저장된다.
    std::vector<std::uint32_t> Indices;

    QuadTreeLODFullResolutionRange FullResolutionRange;
};

class QuadTreeLOD
{
public:
    bool Build(
        const std::vector<Vertex>& sourceVertices,
        std::uint32_t vertexWidth,
        std::uint32_t vertexHeight,
        const QuadTreeLODSettings& settings,
        QuadTreeLODMeshData& outMeshData,
        std::wstring& outErrorMessage);

    const QuadTreeLODNode* GetRoot() const;

    std::uint32_t GetTotalNodeCount() const;
    std::uint32_t GetTotalLeafCount() const;
    std::uint32_t GetMaxDepth() const;

private:
    std::unique_ptr<QuadTreeLODNode> BuildNode(
        const std::vector<Vertex>& sourceVertices,
        std::uint32_t vertexWidth,
        std::uint32_t cellStartX,
        std::uint32_t cellStartZ,
        std::uint32_t cellCountX,
        std::uint32_t cellCountZ,
        std::uint32_t depth,
        const QuadTreeLODSettings& settings,
        QuadTreeLODMeshData& meshData);

    DirectX::BoundingBox CalculateBounds(
        const std::vector<Vertex>& sourceVertices,
        std::uint32_t vertexWidth,
        std::uint32_t cellStartX,
        std::uint32_t cellStartZ,
        std::uint32_t cellCountX,
        std::uint32_t cellCountZ,
        float skirtDepth) const;

    void AppendFullResolutionIndices(
        std::uint32_t vertexWidth,
        std::uint32_t vertexHeight,
        QuadTreeLODMeshData& meshData) const;

    void AppendNodePatch(
        const std::vector<Vertex>& sourceVertices,
        std::uint32_t vertexWidth,
        std::uint32_t cellStartX,
        std::uint32_t cellStartZ,
        std::uint32_t cellCountX,
        std::uint32_t cellCountZ,
        const QuadTreeLODSettings& settings,
        QuadTreeLODNode& node,
        QuadTreeLODMeshData& meshData) const;

    std::vector<std::uint32_t> BuildAxisSamples(
        std::uint32_t start,
        std::uint32_t cellCount,
        std::uint32_t maxSegments) const;

    void AppendSkirtEdge(
        const std::vector<std::uint32_t>& topVertexIndices,
        float skirtDepth,
        QuadTreeLODMeshData& meshData) const;

private:
    std::unique_ptr<QuadTreeLODNode> root_;

    std::uint32_t totalNodeCount_ = 0u;
    std::uint32_t totalLeafCount_ = 0u;
    std::uint32_t maxDepth_ = 0u;
};
