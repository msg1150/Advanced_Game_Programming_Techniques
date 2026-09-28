// ============================================================================
// QuadTreeLODGeomorph.h
// ----------------------------------------------------------------------------
// 기존 QuadTreeLOD 자료구조와 Selector를 수정하지 않고 붙이는 독립 Feature.
//
// 역할은 두 가지다.
// 1) 기존 Node Patch를 Geomorph용 Mesh로 "복제"하면서 각 Vertex에
//    Parent 표면의 목표 높이/Normal을 기록한다.
// 2) Runtime에서 선택된 Node가 Parent Split 경계에 얼마나 가까운지 계산하고
//    Vertex Shader에 Morph Factor를 전달한다.
//
// 중요:
// - 기존 QuadTreeLOD의 Node 분할/LOD 선택/Skirt 생성 규칙은 변경하지 않는다.
// - 기존 Vertex 구조도 변경하지 않는다.
// - Triplanar Terrain에서 사용하지 않던 Vertex Color를
//   Geomorph 전용 Mesh에서만 Morph Target 저장 용도로 사용한다.
//   Color.x   = Parent 표면 목표 높이
//   Color.yzw = Parent 표면 목표 Normal
// - Geomorph Feature를 끄면 같은 Mesh를 기존 Triplanar Shader로 그리므로
//   Color 값은 완전히 무시되고 기존 높이가 그대로 사용된다.
// ============================================================================
#pragma once

#include "Features/QuadTreeLOD/QuadTreeLOD.h"
#include "Features/QuadTreeLODGeomorph/QuadTreeLODGeomorphPolicy.h"
#include "Graphics/ConstantBuffer.h"
#include "Graphics/Shader.h"

#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <d3d11.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct QuadTreeLODGeomorphRangeBinding
{
    // 기존 QuadTreeLODSelector가 반환하는 StartIndex.
    std::uint32_t SourceStartIndex = 0u;

    // Geomorph용 Mesh에서 실제로 Draw할 StartIndex.
    std::uint32_t GeomorphStartIndex = 0u;
    std::uint32_t IndexCount = 0u;

    // Root는 위에 Parent가 없으므로 Morph 대상이 없다.
    bool HasParent = false;

    // Child가 막 선택되는 순간 Parent 표면과 정확히 이어지게 하기 위해
    // Parent Node의 Bounds를 저장한다.
    DirectX::BoundingBox ParentBounds = {};
};

struct QuadTreeLODGeomorphMeshData
{
    // Full Resolution Vertex는 앞쪽에 그대로 보관하고,
    // 그 뒤에 Node별 독립 Vertex Block을 추가한다.
    // Node별로 Vertex를 분리해야 같은 원본 Vertex라도 서로 다른 Parent 목표값을
    // 가질 수 있다.
    std::vector<Vertex> Vertices;
    std::vector<std::uint32_t> Indices;

    QuadTreeLODFullResolutionRange FullResolutionRange;

    // SourceStartIndex 기준으로 정렬된다.
    std::vector<QuadTreeLODGeomorphRangeBinding> Ranges;
};

class QuadTreeLODGeomorphBuilder
{
public:
    // 기존 QuadTreeLOD Build 결과를 입력으로 받아 Geomorph 전용 Mesh를 만든다.
    // 기존 MeshData / Tree를 수정하지 않으므로 실패 시 기존 Mesh로 즉시
    // Fallback할 수 있다.
    static bool Build(
        const std::vector<Vertex>& sourceVertices,
        std::uint32_t vertexWidth,
        std::uint32_t vertexHeight,
        const QuadTreeLOD& tree,
        const QuadTreeLODMeshData& sourceMeshData,
        const QuadTreeLODSettings& lodSettings,
        QuadTreeLODGeomorphMeshData& outData,
        std::wstring& outErrorMessage);

    // Selector가 반환한 기존 StartIndex를 Geomorph Mesh의 StartIndex로 변환한다.
    static const QuadTreeLODGeomorphRangeBinding* FindRange(
        const QuadTreeLODGeomorphMeshData& data,
        std::uint32_t sourceStartIndex);

    // GPU Buffer 생성 후 CPU Vertex/Index는 버리고 Range Mapping만 보관할 수 있도록
    // Mapping 배열만 받는 독립 Overload를 제공한다.
    static const QuadTreeLODGeomorphRangeBinding* FindRange(
        const std::vector<QuadTreeLODGeomorphRangeBinding>& ranges,
        std::uint32_t sourceStartIndex);

    // 선택된 Child Node의 Morph Factor를 계산한다.
    // World Matrix를 받아 Terrain이 나중에 이동/회전/Scale되더라도
    // Parent Bounds 거리 계산은 World Space에서 수행한다.
    static float CalculateMorphFactor(
        const QuadTreeLODGeomorphRangeBinding& binding,
        const DirectX::XMFLOAT3& cameraWorldPosition,
        const DirectX::XMMATRIX& world,
        const QuadTreeLODSettings& lodSettings,
        const QuadTreeLODGeomorphPolicy::Settings& geomorphSettings);

private:
    struct Region
    {
        std::uint32_t StartX = 0u;
        std::uint32_t StartZ = 0u;
        std::uint32_t CountX = 0u;
        std::uint32_t CountZ = 0u;
    };

    struct SurfaceSample
    {
        float Height = 0.0f;
        DirectX::XMFLOAT3 Normal = { 0.0f, 1.0f, 0.0f };
    };

    static bool BuildNode(
        const QuadTreeLODNode& node,
        const QuadTreeLODNode* parentNode,
        const Region& region,
        const Region* parentRegion,
        const std::vector<Vertex>& sourceVertices,
        std::uint32_t vertexWidth,
        std::uint32_t vertexHeight,
        const QuadTreeLODMeshData& sourceMeshData,
        const QuadTreeLODSettings& lodSettings,
        QuadTreeLODGeomorphMeshData& outData,
        std::wstring& outErrorMessage);

    static std::vector<Region> SplitRegionForChildren(
        const Region& region);

    static std::vector<std::uint32_t> BuildAxisSamples(
        std::uint32_t start,
        std::uint32_t cellCount,
        std::uint32_t maxSegments);

    static SurfaceSample EvaluateParentSurface(
        const std::vector<Vertex>& sourceVertices,
        std::uint32_t vertexWidth,
        const Region& parentRegion,
        std::uint32_t leafCellSize,
        std::uint32_t gridX,
        std::uint32_t gridZ);

    static bool PositionToGridCoordinate(
        const std::vector<Vertex>& sourceVertices,
        std::uint32_t vertexWidth,
        std::uint32_t vertexHeight,
        const DirectX::XMFLOAT3& position,
        std::uint32_t& outX,
        std::uint32_t& outZ);

    static float DistanceToBounds(
        const DirectX::XMFLOAT3& point,
        const DirectX::BoundingBox& bounds);
};

// ----------------------------------------------------------------------------
// Geomorph 전용 Shader/Constant Buffer.
// Texture와 Pixel Shader 설정은 기존 TriplanarMaterial::Bind가 먼저 수행하고,
// 이 Renderer가 Vertex Shader만 Geomorph 버전으로 교체한다.
//
// 따라서 Grass/Rock/Snow Texture 관리 코드는 복제하지 않는다.
// ----------------------------------------------------------------------------
class QuadTreeLODGeomorphRenderer
{
public:
    bool Initialize(
        ID3D11Device* device,
        const std::filesystem::path& triplanarShaderDirectory);

    // Geomorph Vertex Shader를 Bind한다.
    // debugVisualization=false : 기존 Triplanar Pixel Shader 사용.
    // debugVisualization=true  : Morph 진행률을 Red -> Yellow -> Green으로 표시.
    // Debug Shader가 없거나 초기화에 실패하면 자동으로 일반 Geomorph Shader로 Fallback한다.
    void Begin(
        ID3D11DeviceContext* context,
        bool debugVisualization) const;

    // DrawRange 직전에 Node별 Morph Factor만 갱신한다.
    void SetMorphFactor(
        ID3D11DeviceContext* context,
        float factor);

    const std::wstring& GetLastErrorMessage() const;

private:
    struct CBGeomorph
    {
        float MorphFactor = 0.0f;
        DirectX::XMFLOAT3 Padding = { 0.0f, 0.0f, 0.0f };
    };

    // 일반 Geomorph 렌더링과 Debug 색상 렌더링은 같은 Vertex Shader/CB를 공유한다.
    // Debug Shader는 선택 기능이므로 실패해도 일반 Geomorphing은 계속 사용할 수 있다.
    Shader shader_;
    Shader debugShader_;
    bool debugShaderAvailable_ = false;

    ConstantBuffer<CBGeomorph> morphBuffer_;
    std::wstring lastErrorMessage_;
};
