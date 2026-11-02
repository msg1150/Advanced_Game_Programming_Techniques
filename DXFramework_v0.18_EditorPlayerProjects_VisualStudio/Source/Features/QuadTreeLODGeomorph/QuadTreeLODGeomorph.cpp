// ============================================================================
// QuadTreeLODGeomorph.cpp
// ============================================================================
#include "Features/QuadTreeLODGeomorph/QuadTreeLODGeomorph.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

using namespace DirectX;

namespace
{
    // Vec3 선형 보간을 작은 지역 함수로 둔다.
    XMFLOAT3 WeightedNormal(
        const XMFLOAT3& a, float wa,
        const XMFLOAT3& b, float wb,
        const XMFLOAT3& c, float wc)
    {
        XMFLOAT3 result =
        {
            a.x * wa + b.x * wb + c.x * wc,
            a.y * wa + b.y * wb + c.y * wc,
            a.z * wa + b.z * wb + c.z * wc
        };

        const float length =
            std::sqrt(
                result.x * result.x +
                result.y * result.y +
                result.z * result.z);

        if (length > 0.000001f)
        {
            result.x /= length;
            result.y /= length;
            result.z /= length;
        }
        else
        {
            result = { 0.0f, 1.0f, 0.0f };
        }

        return result;
    }


}

bool QuadTreeLODGeomorphBuilder::Build(
    const std::vector<Vertex>& sourceVertices,
    std::uint32_t vertexWidth,
    std::uint32_t vertexHeight,
    const QuadTreeLOD& tree,
    const QuadTreeLODMeshData& sourceMeshData,
    const QuadTreeLODSettings& lodSettings,
    QuadTreeLODGeomorphMeshData& outData,
    std::wstring& outErrorMessage)
{
    outData = {};
    outErrorMessage.clear();

    const QuadTreeLODNode* root = tree.GetRoot();

    if (!root ||
        vertexWidth < 2u ||
        vertexHeight < 2u ||
        sourceVertices.size() !=
            static_cast<std::size_t>(vertexWidth) * vertexHeight)
    {
        outErrorMessage = L"Geomorph 입력 Terrain Grid가 올바르지 않습니다.";
        return false;
    }

    if (sourceMeshData.FullResolutionRange.IndexCount == 0u ||
        sourceMeshData.Indices.empty())
    {
        outErrorMessage = L"Geomorph 원본 LOD MeshData가 비어 있습니다.";
        return false;
    }

    // ------------------------------------------------------------------------
    // Full Resolution 경로는 기존 Vertex/Index를 그대로 사용한다.
    // LOD를 끈 경우에는 Geomorph Shader 자체를 사용하지 않으므로
    // 기존 Full Resolution Terrain과 완전히 같은 결과를 유지한다.
    // ------------------------------------------------------------------------
    outData.Vertices = sourceVertices;
    outData.FullResolutionRange.StartIndex = 0u;
    outData.FullResolutionRange.IndexCount =
        sourceMeshData.FullResolutionRange.IndexCount;
    outData.FullResolutionRange.SurfaceTriangleCount =
        sourceMeshData.FullResolutionRange.SurfaceTriangleCount;

    const std::size_t fullBegin =
        sourceMeshData.FullResolutionRange.StartIndex;
    const std::size_t fullEnd =
        fullBegin + sourceMeshData.FullResolutionRange.IndexCount;

    if (fullEnd > sourceMeshData.Indices.size())
    {
        outErrorMessage = L"Geomorph Full Resolution Index 범위 오류입니다.";
        return false;
    }

    outData.Indices.insert(
        outData.Indices.end(),
        sourceMeshData.Indices.begin() + fullBegin,
        sourceMeshData.Indices.begin() + fullEnd);

    const Region rootRegion =
    {
        0u,
        0u,
        vertexWidth - 1u,
        vertexHeight - 1u
    };

    if (!BuildNode(
            *root,
            nullptr,
            rootRegion,
            nullptr,
            sourceVertices,
            vertexWidth,
            vertexHeight,
            sourceMeshData,
            lodSettings,
            outData,
            outErrorMessage))
    {
        return false;
    }

    std::sort(
        outData.Ranges.begin(),
        outData.Ranges.end(),
        [](const auto& a, const auto& b)
        {
            return a.SourceStartIndex < b.SourceStartIndex;
        });

    return true;
}

bool QuadTreeLODGeomorphBuilder::BuildNode(
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
    std::wstring& outErrorMessage)
{
    const std::size_t sourceBegin = node.StartIndex;
    const std::size_t sourceEnd = sourceBegin + node.IndexCount;

    if (sourceEnd > sourceMeshData.Indices.size())
    {
        outErrorMessage = L"Geomorph Node Index 범위 오류입니다.";
        return false;
    }

    QuadTreeLODGeomorphRangeBinding binding;
    binding.SourceStartIndex = node.StartIndex;
    binding.GeomorphStartIndex =
        static_cast<std::uint32_t>(outData.Indices.size());
    binding.IndexCount = node.IndexCount;
    binding.HasParent = parentNode != nullptr;

    if (parentNode)
    {
        binding.ParentBounds = parentNode->Bounds;
    }

    // 같은 Node Range 안에서는 같은 원본 Vertex를 한 번만 복제한다.
    // 다른 Node는 Parent 목표 표면이 다를 수 있으므로 서로 공유하지 않는다.
    std::unordered_map<std::uint32_t, std::uint32_t> remap;
    remap.reserve(node.IndexCount);

    for (std::size_t indexPosition = sourceBegin;
         indexPosition < sourceEnd;
         ++indexPosition)
    {
        const std::uint32_t sourceIndex =
            sourceMeshData.Indices[indexPosition];

        if (sourceIndex >= sourceMeshData.Vertices.size())
        {
            outErrorMessage = L"Geomorph Node Vertex Index 범위 오류입니다.";
            return false;
        }

        auto found = remap.find(sourceIndex);

        if (found != remap.end())
        {
            outData.Indices.push_back(found->second);
            continue;
        }

        Vertex vertex = sourceMeshData.Vertices[sourceIndex];

        // --------------------------------------------------------------------
        // Morph Target 계산
        // --------------------------------------------------------------------
        // Root는 Parent가 없으므로 자기 자신을 Target으로 기록한다.
        // Child부터는 Parent Patch의 실제 삼각형 표면을 Barycentric 보간해서
        // 같은 XZ 위치의 목표 높이와 Normal을 구한다.
        //
        // Skirt Vertex도 XZ가 원본 경계 Vertex와 같으므로 Grid 좌표를 찾은 뒤,
        // 원래 Skirt가 내려가 있던 Y Offset을 Parent 표면에도 그대로 적용한다.
        // --------------------------------------------------------------------
        float targetHeight = vertex.Position.y;
        XMFLOAT3 targetNormal = vertex.Normal;

        if (parentNode && parentRegion)
        {
            std::uint32_t gridX = 0u;
            std::uint32_t gridZ = 0u;

            if (!PositionToGridCoordinate(
                    sourceVertices,
                    vertexWidth,
                    vertexHeight,
                    vertex.Position,
                    gridX,
                    gridZ))
            {
                outErrorMessage = L"Geomorph Vertex의 Grid 좌표를 찾지 못했습니다.";
                return false;
            }

            const std::uint32_t topIndex =
                gridZ * vertexWidth + gridX;

            if (topIndex >= sourceVertices.size())
            {
                outErrorMessage = L"Geomorph 원본 Surface Vertex 범위 오류입니다.";
                return false;
            }

            const float skirtOffset =
                vertex.Position.y - sourceVertices[topIndex].Position.y;

            const SurfaceSample parentSample =
                EvaluateParentSurface(
                    sourceVertices,
                    vertexWidth,
                    *parentRegion,
                    lodSettings.LeafCellSize,
                    gridX,
                    gridZ);

            targetHeight = parentSample.Height + skirtOffset;
            targetNormal = parentSample.Normal;
        }

        // Triplanar Terrain에서는 Vertex Color를 사용하지 않으므로
        // Geomorph 전용 Mesh에 한해서 안전하게 Morph Target 저장소로 사용한다.
        vertex.Color =
        {
            targetHeight,
            targetNormal.x,
            targetNormal.y,
            targetNormal.z
        };

        const std::uint32_t newIndex =
            static_cast<std::uint32_t>(outData.Vertices.size());

        outData.Vertices.push_back(vertex);
        remap.emplace(sourceIndex, newIndex);
        outData.Indices.push_back(newIndex);
    }

    outData.Ranges.push_back(binding);

    if (node.IsLeaf())
    {
        return true;
    }

    const auto childRegions = SplitRegionForChildren(region);
    std::size_t childRegionIndex = 0u;

    for (const auto& child : node.Children)
    {
        if (!child)
        {
            continue;
        }

        if (childRegionIndex >= childRegions.size())
        {
            outErrorMessage = L"Geomorph QuadTree Child Region 개수가 일치하지 않습니다.";
            return false;
        }

        if (!BuildNode(
                *child,
                &node,
                childRegions[childRegionIndex],
                &region,
                sourceVertices,
                vertexWidth,
                vertexHeight,
                sourceMeshData,
                lodSettings,
                outData,
                outErrorMessage))
        {
            return false;
        }

        ++childRegionIndex;
    }

    return true;
}

std::vector<QuadTreeLODGeomorphBuilder::Region>
QuadTreeLODGeomorphBuilder::SplitRegionForChildren(
    const Region& region)
{
    std::vector<Region> result;

    std::uint32_t xStarts[2] = { region.StartX, region.StartX };
    std::uint32_t xCounts[2] = { region.CountX, 0u };
    std::uint32_t xParts = 1u;

    if (region.CountX > 1u)
    {
        const std::uint32_t first = region.CountX / 2u;
        const std::uint32_t second = region.CountX - first;
        xStarts[0] = region.StartX;
        xStarts[1] = region.StartX + first;
        xCounts[0] = first;
        xCounts[1] = second;
        xParts = 2u;
    }

    std::uint32_t zStarts[2] = { region.StartZ, region.StartZ };
    std::uint32_t zCounts[2] = { region.CountZ, 0u };
    std::uint32_t zParts = 1u;

    if (region.CountZ > 1u)
    {
        const std::uint32_t first = region.CountZ / 2u;
        const std::uint32_t second = region.CountZ - first;
        zStarts[0] = region.StartZ;
        zStarts[1] = region.StartZ + first;
        zCounts[0] = first;
        zCounts[1] = second;
        zParts = 2u;
    }

    for (std::uint32_t z = 0u; z < zParts; ++z)
    {
        for (std::uint32_t x = 0u; x < xParts; ++x)
        {
            result.push_back(
                {
                    xStarts[x],
                    zStarts[z],
                    xCounts[x],
                    zCounts[z]
                });
        }
    }

    return result;
}

std::vector<std::uint32_t> QuadTreeLODGeomorphBuilder::BuildAxisSamples(
    std::uint32_t start,
    std::uint32_t cellCount,
    std::uint32_t maxSegments)
{
    std::vector<std::uint32_t> samples;

    if (cellCount == 0u || maxSegments == 0u)
    {
        return samples;
    }

    const std::uint32_t segmentCount =
        std::min(cellCount, maxSegments);

    samples.reserve(static_cast<std::size_t>(segmentCount) + 1u);

    for (std::uint32_t index = 0u;
         index <= segmentCount;
         ++index)
    {
        const std::uint64_t numerator =
            static_cast<std::uint64_t>(index) * cellCount;

        const std::uint32_t coordinate =
            start + static_cast<std::uint32_t>(numerator / segmentCount);

        if (samples.empty() || samples.back() != coordinate)
        {
            samples.push_back(coordinate);
        }
    }

    const std::uint32_t end = start + cellCount;

    if (samples.empty() || samples.back() != end)
    {
        samples.push_back(end);
    }

    return samples;
}

QuadTreeLODGeomorphBuilder::SurfaceSample
QuadTreeLODGeomorphBuilder::EvaluateParentSurface(
    const std::vector<Vertex>& sourceVertices,
    std::uint32_t vertexWidth,
    const Region& parentRegion,
    std::uint32_t leafCellSize,
    std::uint32_t gridX,
    std::uint32_t gridZ)
{
    const auto xSamples =
        BuildAxisSamples(
            parentRegion.StartX,
            parentRegion.CountX,
            leafCellSize);

    const auto zSamples =
        BuildAxisSamples(
            parentRegion.StartZ,
            parentRegion.CountZ,
            leafCellSize);

    const auto findSegment = [](
        const std::vector<std::uint32_t>& samples,
        std::uint32_t coordinate) -> std::size_t
    {
        if (samples.size() < 2u)
        {
            return 0u;
        }

        auto upper = std::upper_bound(
            samples.begin(),
            samples.end(),
            coordinate);

        if (upper == samples.begin())
        {
            return 0u;
        }

        if (upper == samples.end())
        {
            return samples.size() - 2u;
        }

        return static_cast<std::size_t>(
            std::distance(samples.begin(), upper) - 1);
    };

    const std::size_t xSegment = findSegment(xSamples, gridX);
    const std::size_t zSegment = findSegment(zSamples, gridZ);

    const std::uint32_t x0 = xSamples[xSegment];
    const std::uint32_t x1 = xSamples[xSegment + 1u];
    const std::uint32_t z0 = zSamples[zSegment];
    const std::uint32_t z1 = zSamples[zSegment + 1u];

    const float tx =
        x1 > x0
        ? static_cast<float>(gridX - x0) / static_cast<float>(x1 - x0)
        : 0.0f;

    const float tz =
        z1 > z0
        ? static_cast<float>(gridZ - z0) / static_cast<float>(z1 - z0)
        : 0.0f;

    const Vertex& topLeft = sourceVertices[z0 * vertexWidth + x0];
    const Vertex& topRight = sourceVertices[z0 * vertexWidth + x1];
    const Vertex& bottomLeft = sourceVertices[z1 * vertexWidth + x0];
    const Vertex& bottomRight = sourceVertices[z1 * vertexWidth + x1];

    SurfaceSample result;

    // 기존 QuadTreeLOD::AppendNodePatch의 Triangle 분할과 동일한 대각선을 사용한다.
    // 첫 Triangle: TL, BL, TR
    // 둘째 Triangle: TR, BL, BR
    if (tx + tz <= 1.0f)
    {
        const float wTL = 1.0f - tx - tz;
        const float wTR = tx;
        const float wBL = tz;

        result.Height =
            topLeft.Position.y * wTL +
            topRight.Position.y * wTR +
            bottomLeft.Position.y * wBL;

        result.Normal =
            WeightedNormal(
                topLeft.Normal, wTL,
                topRight.Normal, wTR,
                bottomLeft.Normal, wBL);
    }
    else
    {
        const float wTR = 1.0f - tz;
        const float wBL = 1.0f - tx;
        const float wBR = tx + tz - 1.0f;

        result.Height =
            topRight.Position.y * wTR +
            bottomLeft.Position.y * wBL +
            bottomRight.Position.y * wBR;

        result.Normal =
            WeightedNormal(
                topRight.Normal, wTR,
                bottomLeft.Normal, wBL,
                bottomRight.Normal, wBR);
    }

    return result;
}

bool QuadTreeLODGeomorphBuilder::PositionToGridCoordinate(
    const std::vector<Vertex>& sourceVertices,
    std::uint32_t vertexWidth,
    std::uint32_t vertexHeight,
    const XMFLOAT3& position,
    std::uint32_t& outX,
    std::uint32_t& outZ)
{
    if (vertexWidth < 2u ||
        vertexHeight < 2u ||
        sourceVertices.size() <
            static_cast<std::size_t>(vertexWidth) * vertexHeight)
    {
        return false;
    }

    const float originX = sourceVertices[0].Position.x;
    const float originZ = sourceVertices[0].Position.z;
    const float stepX = sourceVertices[1].Position.x - originX;
    const float stepZ = sourceVertices[vertexWidth].Position.z - originZ;

    if (std::abs(stepX) < 0.000001f ||
        std::abs(stepZ) < 0.000001f)
    {
        return false;
    }

    const long long x =
        std::llround((position.x - originX) / stepX);
    const long long z =
        std::llround((position.z - originZ) / stepZ);

    if (x < 0 ||
        z < 0 ||
        x >= static_cast<long long>(vertexWidth) ||
        z >= static_cast<long long>(vertexHeight))
    {
        return false;
    }

    outX = static_cast<std::uint32_t>(x);
    outZ = static_cast<std::uint32_t>(z);
    return true;
}

const QuadTreeLODGeomorphRangeBinding* QuadTreeLODGeomorphBuilder::FindRange(
    const QuadTreeLODGeomorphMeshData& data,
    std::uint32_t sourceStartIndex)
{
    return FindRange(data.Ranges, sourceStartIndex);
}

const QuadTreeLODGeomorphRangeBinding* QuadTreeLODGeomorphBuilder::FindRange(
    const std::vector<QuadTreeLODGeomorphRangeBinding>& ranges,
    std::uint32_t sourceStartIndex)
{
    const auto found =
        std::lower_bound(
            ranges.begin(),
            ranges.end(),
            sourceStartIndex,
            [](const QuadTreeLODGeomorphRangeBinding& binding,
               std::uint32_t value)
            {
                return binding.SourceStartIndex < value;
            });

    if (found == ranges.end() ||
        found->SourceStartIndex != sourceStartIndex)
    {
        return nullptr;
    }

    return &(*found);
}

float QuadTreeLODGeomorphBuilder::DistanceToBounds(
    const XMFLOAT3& point,
    const BoundingBox& bounds)
{
    const float dx =
        std::max(
            std::abs(point.x - bounds.Center.x) - bounds.Extents.x,
            0.0f);

    const float dy =
        std::max(
            std::abs(point.y - bounds.Center.y) - bounds.Extents.y,
            0.0f);

    const float dz =
        std::max(
            std::abs(point.z - bounds.Center.z) - bounds.Extents.z,
            0.0f);

    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float QuadTreeLODGeomorphBuilder::CalculateMorphFactor(
    const QuadTreeLODGeomorphRangeBinding& binding,
    const XMFLOAT3& cameraWorldPosition,
    const XMMATRIX& world,
    const QuadTreeLODSettings& lodSettings,
    const QuadTreeLODGeomorphPolicy::Settings& geomorphSettings)
{
    if (!binding.HasParent ||
        !QuadTreeLODGeomorphPolicy::IsValid(geomorphSettings))
    {
        return 0.0f;
    }

    BoundingBox worldParentBounds;
    binding.ParentBounds.Transform(worldParentBounds, world);

    const float distance =
        DistanceToBounds(cameraWorldPosition, worldParentBounds);

    // 기존 Selector와 동일하게 XZ Node Size만 사용한다.
    const float parentWorldSize =
        std::max(
            worldParentBounds.Extents.x * 2.0f,
            worldParentBounds.Extents.z * 2.0f);

    const float parentSplitDistance =
        parentWorldSize * lodSettings.SplitDistanceFactor;

    return QuadTreeLODGeomorphPolicy::CalculateMorphFactor(
        distance,
        parentSplitDistance,
        geomorphSettings.TransitionRatio);
}

bool QuadTreeLODGeomorphRenderer::Initialize(
    ID3D11Device* device,
    const std::filesystem::path& triplanarShaderDirectory)
{
    lastErrorMessage_.clear();

    if (!device)
    {
        lastErrorMessage_ = L"Geomorph Renderer Device가 nullptr입니다.";
        return false;
    }

    if (!shader_.Initialize(
            device,
            triplanarShaderDirectory / L"TriplanarGeomorphVS.hlsl",
            triplanarShaderDirectory / L"TriplanarPS.hlsl"))
    {
        lastErrorMessage_ =
            L"Geomorph Shader 초기화 실패\n" +
            shader_.GetLastErrorMessage();
        return false;
    }

    // ------------------------------------------------------------------------
    // Debug Visualization Shader
    // ------------------------------------------------------------------------
    // Debug 기능은 확인용 보조 기능이다. 따라서 Debug PS 파일이 없거나
    // 컴파일에 실패해도 Geomorphing 자체를 실패시키지 않는다.
    // 이 구조 덕분에 Debug Visualization만 제거해도 기존 Geomorph 경로는 유지된다.
    debugShaderAvailable_ =
        debugShader_.Initialize(
            device,
            triplanarShaderDirectory / L"TriplanarGeomorphVS.hlsl",
            triplanarShaderDirectory / L"TriplanarGeomorphDebugPS.hlsl");

    if (!morphBuffer_.Initialize(device))
    {
        lastErrorMessage_ = L"Geomorph Constant Buffer 생성 실패";
        return false;
    }

    return true;
}

void QuadTreeLODGeomorphRenderer::Begin(
    ID3D11DeviceContext* context,
    bool debugVisualization) const
{
    if (!context)
    {
        return;
    }

    // Debug가 요청됐고 Debug Shader가 준비된 경우에만 색상 시각화 Shader를 사용한다.
    // 그렇지 않으면 기존 Triplanar Geomorph Shader로 자동 Fallback한다.
    if (debugVisualization && debugShaderAvailable_)
    {
        debugShader_.Bind(context);
    }
    else
    {
        shader_.Bind(context);
    }
}

void QuadTreeLODGeomorphRenderer::SetMorphFactor(
    ID3D11DeviceContext* context,
    float factor)
{
    if (!context)
    {
        return;
    }

    CBGeomorph data;
    data.MorphFactor = std::clamp(factor, 0.0f, 1.0f);

    morphBuffer_.Update(context, data);

    // b0 = Transform, b1 = Pixel Triplanar Settings.
    // Geomorph 전용 Vertex Constant는 충돌하지 않도록 b2를 사용한다.
    morphBuffer_.BindVS(context, 2u);
}

const std::wstring& QuadTreeLODGeomorphRenderer::GetLastErrorMessage() const
{
    return lastErrorMessage_;
}
