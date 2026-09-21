// ============================================================================
// QuadTree.h
// ----------------------------------------------------------------------------
// Grid Terrain의 XZ Cell 영역을 재귀적으로 4분할하는 CPU QuadTree.
//
// QuadTree는 Camera, Shader, GPU를 알지 못한다.
// 다음 정보만 보관한다.
//
// - 각 Node의 Terrain AABB
// - 하위 Child Node
// - 해당 Node가 담당하는 Index Buffer 연속 범위
// - 하위 Leaf 개수
//
// Index Buffer는 QuadTree Leaf 순서대로 재배치한다.
// 따라서 Parent의 모든 하위 Leaf Index도 하나의 연속 Range가 된다.
// Frustum이 Parent를 완전히 포함하면 Child를 검사하지 않고
// Parent Range를 DrawIndexed 한 번으로 그릴 수 있다.
// ============================================================================

#pragma once

#include "Graphics/Mesh.h"

#include <DirectXCollision.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct QuadTreeSettings
{
    // Leaf Node 하나가 한 축에서 담당할 최대 Terrain Cell 수.
    std::uint32_t LeafCellSize = 8u;
};

struct QuadTreeNode
{
    DirectX::BoundingBox Bounds;

    // 이 Node 이하의 모든 Triangle이 Index Buffer에서 차지하는 연속 범위.
    std::uint32_t StartIndex = 0u;
    std::uint32_t IndexCount = 0u;

    // 이 Node 하위의 최종 Leaf 개수.
    std::uint32_t LeafCount = 0u;

    // nullptr가 아닌 Child만 사용한다.
    std::array<std::unique_ptr<QuadTreeNode>, 4> Children;

    bool IsLeaf() const;
};

class QuadTree
{
public:
    // Grid Vertex를 기반으로 QuadTree를 만들고,
    // Leaf 순서대로 재배치된 Index Buffer를 outIndices에 반환한다.
    bool Build(
        const std::vector<Vertex>& vertices,
        std::uint32_t vertexWidth,
        std::uint32_t vertexHeight,
        const QuadTreeSettings& settings,
        std::vector<std::uint32_t>& outIndices,
        std::wstring& outErrorMessage);

    const QuadTreeNode* GetRoot() const;

    std::uint32_t GetTotalNodeCount() const;
    std::uint32_t GetTotalLeafCount() const;
    std::uint32_t GetTotalIndexCount() const;

private:
    std::unique_ptr<QuadTreeNode> BuildNode(
        const std::vector<Vertex>& vertices,
        std::uint32_t vertexWidth,
        std::uint32_t cellStartX,
        std::uint32_t cellStartZ,
        std::uint32_t cellCountX,
        std::uint32_t cellCountZ,
        std::uint32_t leafCellSize,
        std::vector<std::uint32_t>& outIndices);

    DirectX::BoundingBox CalculateBounds(
        const std::vector<Vertex>& vertices,
        std::uint32_t vertexWidth,
        std::uint32_t cellStartX,
        std::uint32_t cellStartZ,
        std::uint32_t cellCountX,
        std::uint32_t cellCountZ) const;

    void AppendLeafIndices(
        std::uint32_t vertexWidth,
        std::uint32_t cellStartX,
        std::uint32_t cellStartZ,
        std::uint32_t cellCountX,
        std::uint32_t cellCountZ,
        std::vector<std::uint32_t>& outIndices) const;

private:
    std::unique_ptr<QuadTreeNode> root_;

    std::uint32_t totalNodeCount_ = 0u;
    std::uint32_t totalLeafCount_ = 0u;
    std::uint32_t totalIndexCount_ = 0u;
};
