// ============================================================================
// HeightMapTerrainGenerator.cpp
// ============================================================================

#include "Features/HeightMapTerrain/HeightMapTerrainGenerator.h"

#include <DirectXMath.h>

#include <algorithm>
#include <limits>

using namespace DirectX;

bool HeightMapTerrainGenerator::Generate(
    const HeightMapImage& heightMapImage,
    const HeightMapTerrainSettings& settings,
    HeightMapTerrainMeshData& outMeshData,
    std::wstring& outErrorMessage)
{
    outMeshData.Vertices.clear();
    outMeshData.Indices.clear();
    outErrorMessage.clear();

    const std::uint32_t imageWidth =
        heightMapImage.GetWidth();

    const std::uint32_t imageHeight =
        heightMapImage.GetHeight();

    if (imageWidth < 2u ||
        imageHeight < 2u)
    {
        outErrorMessage =
            L"HeightMap 이미지 크기는 최소 2x2 이상이어야 합니다.";

        return false;
    }

    if (settings.CellSize <= 0.0f)
    {
        outErrorMessage =
            L"HeightMapTerrain CellSize는 0보다 커야 합니다.";

        return false;
    }

    if (settings.MaxHeight < settings.MinHeight)
    {
        outErrorMessage =
            L"HeightMapTerrain MaxHeight는 MinHeight보다 크거나 같아야 합니다.";

        return false;
    }

    const std::uint64_t vertexCount =
        static_cast<std::uint64_t>(imageWidth) *
        static_cast<std::uint64_t>(imageHeight);

    if (vertexCount >
        static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        outErrorMessage =
            L"HeightMap Terrain Vertex 개수가 uint32 Index 범위를 초과했습니다.";

        return false;
    }

    const std::uint64_t cellCountX =
        static_cast<std::uint64_t>(imageWidth - 1u);

    const std::uint64_t cellCountZ =
        static_cast<std::uint64_t>(imageHeight - 1u);

    const std::uint64_t indexCount =
        cellCountX *
        cellCountZ *
        6ull;

    outMeshData.Vertices.reserve(
        static_cast<std::size_t>(vertexCount));

    outMeshData.Indices.reserve(
        static_cast<std::size_t>(indexCount));

    const float terrainWidth =
        static_cast<float>(imageWidth - 1u) *
        settings.CellSize;

    const float terrainDepth =
        static_cast<float>(imageHeight - 1u) *
        settings.CellSize;

    const float halfWidth =
        terrainWidth * 0.5f;

    const float halfDepth =
        terrainDepth * 0.5f;

    // ------------------------------------------------------------------------
    // Vertex 생성
    // ------------------------------------------------------------------------
    for (std::uint32_t z = 0u;
         z < imageHeight;
         ++z)
    {
        for (std::uint32_t x = 0u;
             x < imageWidth;
             ++x)
        {
            const float normalizedHeight =
                heightMapImage.GetHeightValue(x, z);

            const float worldHeight =
                settings.MinHeight +
                normalizedHeight *
                (settings.MaxHeight - settings.MinHeight);

            const float positionX =
                static_cast<float>(x) *
                settings.CellSize -
                halfWidth;

            const float positionZ =
                static_cast<float>(z) *
                settings.CellSize -
                halfDepth;

            const float u =
                static_cast<float>(x) /
                static_cast<float>(imageWidth - 1u);

            const float v =
                static_cast<float>(z) /
                static_cast<float>(imageHeight - 1u);

            Vertex vertex = {};
            vertex.Position = { positionX, worldHeight, positionZ };
            vertex.Normal = { 0.0f, 0.0f, 0.0f };
            vertex.UV = { u, v };

            // HeightMap Terrain은 Perlin Terrain과 구분되도록 약간 따뜻한 톤을 사용한다.
            vertex.Color = { 0.95f, 0.88f, 0.62f, 1.0f };

            outMeshData.Vertices.push_back(vertex);
        }
    }

    // ------------------------------------------------------------------------
    // Index 생성
    //
    // 한 픽셀 간격(사각형)을 Triangle 2개로 분할한다.
    // Winding은 +Y를 Front Face로 보이게 맞춘다.
    // ------------------------------------------------------------------------
    for (std::uint32_t z = 0u;
         z < imageHeight - 1u;
         ++z)
    {
        for (std::uint32_t x = 0u;
             x < imageWidth - 1u;
             ++x)
        {
            const std::uint32_t topLeft =
                z * imageWidth +
                x;

            const std::uint32_t topRight =
                topLeft + 1u;

            const std::uint32_t bottomLeft =
                (z + 1u) * imageWidth +
                x;

            const std::uint32_t bottomRight =
                bottomLeft + 1u;

            outMeshData.Indices.push_back(topLeft);
            outMeshData.Indices.push_back(bottomLeft);
            outMeshData.Indices.push_back(topRight);

            outMeshData.Indices.push_back(topRight);
            outMeshData.Indices.push_back(bottomLeft);
            outMeshData.Indices.push_back(bottomRight);
        }
    }

    CalculateNormals(
        outMeshData.Vertices,
        outMeshData.Indices);

    return true;
}

void HeightMapTerrainGenerator::CalculateNormals(
    std::vector<Vertex>& vertices,
    const std::vector<std::uint32_t>& indices)
{
    for (Vertex& vertex : vertices)
    {
        vertex.Normal = { 0.0f, 0.0f, 0.0f };
    }

    for (std::size_t triangleIndex = 0u;
         triangleIndex + 2u < indices.size();
         triangleIndex += 3u)
    {
        const std::uint32_t index0 = indices[triangleIndex + 0u];
        const std::uint32_t index1 = indices[triangleIndex + 1u];
        const std::uint32_t index2 = indices[triangleIndex + 2u];

        const XMVECTOR position0 = XMLoadFloat3(&vertices[index0].Position);
        const XMVECTOR position1 = XMLoadFloat3(&vertices[index1].Position);
        const XMVECTOR position2 = XMLoadFloat3(&vertices[index2].Position);

        const XMVECTOR edge01 = XMVectorSubtract(position1, position0);
        const XMVECTOR edge02 = XMVectorSubtract(position2, position0);

        const XMVECTOR faceNormal = XMVector3Cross(edge01, edge02);

        XMFLOAT3 faceNormalFloat = {};
        XMStoreFloat3(&faceNormalFloat, faceNormal);

        auto accumulate = [&faceNormalFloat](Vertex& vertex)
        {
            vertex.Normal.x += faceNormalFloat.x;
            vertex.Normal.y += faceNormalFloat.y;
            vertex.Normal.z += faceNormalFloat.z;
        };

        accumulate(vertices[index0]);
        accumulate(vertices[index1]);
        accumulate(vertices[index2]);
    }

    constexpr float epsilon = 0.000001f;

    for (Vertex& vertex : vertices)
    {
        XMVECTOR normal = XMLoadFloat3(&vertex.Normal);

        const float lengthSquared =
            XMVectorGetX(XMVector3LengthSq(normal));

        if (lengthSquared <= epsilon)
        {
            vertex.Normal = { 0.0f, 1.0f, 0.0f };
            continue;
        }

        normal = XMVector3Normalize(normal);
        XMStoreFloat3(&vertex.Normal, normal);
    }
}
