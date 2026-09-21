// ============================================================================
// PerlinTerrainGenerator.cpp
// ============================================================================

#include "Features/PerlinTerrain/PerlinTerrainGenerator.h"

#include "Features/PerlinTerrain/PerlinNoise.h"

#include <DirectXMath.h>

#include <algorithm>
#include <limits>

using namespace DirectX;

bool PerlinTerrainGenerator::Generate(
    const PerlinTerrainSettings& settings,
    PerlinTerrainMeshData& outMeshData,
    std::wstring& outErrorMessage)
{
    outMeshData.Vertices.clear();
    outMeshData.Indices.clear();
    outErrorMessage.clear();

    // ------------------------------------------------------------------------
    // 입력값 검증
    // ------------------------------------------------------------------------
    if (settings.CellsX == 0u ||
        settings.CellsZ == 0u)
    {
        outErrorMessage =
            L"Terrain Cell 개수는 1 이상이어야 합니다.";

        return false;
    }

    if (settings.CellSize <= 0.0f)
    {
        outErrorMessage =
            L"Terrain CellSize는 0보다 커야 합니다.";

        return false;
    }

    if (settings.NoiseFrequency <= 0.0f)
    {
        outErrorMessage =
            L"Perlin NoiseFrequency는 0보다 커야 합니다.";

        return false;
    }

    if (settings.HeightScale < 0.0f)
    {
        outErrorMessage =
            L"Terrain HeightScale은 음수가 될 수 없습니다.";

        return false;
    }

    // Cell이 N개라면 Vertex는 N + 1개가 필요하다.
    const std::uint64_t vertexCountX =
        static_cast<std::uint64_t>(settings.CellsX) +
        1ull;

    const std::uint64_t vertexCountZ =
        static_cast<std::uint64_t>(settings.CellsZ) +
        1ull;

    const std::uint64_t totalVertexCount =
        vertexCountX *
        vertexCountZ;

    // 현재 Framework의 Index가 uint32_t이므로
    // Vertex 인덱스가 표현 가능한 범위를 넘지 않도록 방어한다.
    if (totalVertexCount >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::uint32_t>::max()))
    {
        outErrorMessage =
            L"Terrain Vertex 개수가 uint32 Index 범위를 초과했습니다.";

        return false;
    }

    const std::uint64_t totalIndexCount =
        static_cast<std::uint64_t>(settings.CellsX) *
        static_cast<std::uint64_t>(settings.CellsZ) *
        6ull;

    // ------------------------------------------------------------------------
    // 재할당 횟수를 줄이기 위해 필요한 크기를 미리 확보한다.
    // ------------------------------------------------------------------------
    outMeshData.Vertices.reserve(
        static_cast<std::size_t>(totalVertexCount));

    outMeshData.Indices.reserve(
        static_cast<std::size_t>(totalIndexCount));

    // Seed에 따라 결정적인 Perlin Noise Generator를 만든다.
    const PerlinNoise perlinNoise(
        settings.Seed);

    // Terrain의 중심이 World 원점에 오도록 절반 크기를 계산한다.
    const float terrainWidth =
        static_cast<float>(settings.CellsX) *
        settings.CellSize;

    const float terrainDepth =
        static_cast<float>(settings.CellsZ) *
        settings.CellSize;

    const float halfWidth =
        terrainWidth * 0.5f;

    const float halfDepth =
        terrainDepth * 0.5f;

    // ------------------------------------------------------------------------
    // Vertex 생성
    //
    // 좌표:
    // X = 가로
    // Y = Perlin Noise Height
    // Z = 세로
    //
    // Noise Sampling은 Vertex 배열 인덱스가 아니라
    // 실제 Local/World 공간에 해당하는 X/Z 좌표를 사용한다.
    //
    // 이 방식은 이후 Terrain을 Chunk로 분할해도
    // 같은 좌표를 Sampling하면 경계가 자연스럽게 이어질 수 있게 한다.
    // ------------------------------------------------------------------------
    for (std::uint32_t z = 0u;
         z <= settings.CellsZ;
         ++z)
    {
        for (std::uint32_t x = 0u;
             x <= settings.CellsX;
             ++x)
        {
            const float positionX =
                static_cast<float>(x) *
                settings.CellSize -
                halfWidth;

            const float positionZ =
                static_cast<float>(z) *
                settings.CellSize -
                halfDepth;

            const float noiseValue =
                perlinNoise.Noise(
                    positionX *
                        settings.NoiseFrequency,
                    positionZ *
                        settings.NoiseFrequency);

            const float height =
                noiseValue *
                settings.HeightScale;

            // UV는 Terrain 전체를 0~1 범위로 사용한다.
            // 현재 Texture는 없지만 이후 바로 활용할 수 있게 미리 생성한다.
            const float u =
                static_cast<float>(x) /
                static_cast<float>(settings.CellsX);

            const float v =
                static_cast<float>(z) /
                static_cast<float>(settings.CellsZ);

            Vertex vertex = {};

            vertex.Position =
            {
                positionX,
                height,
                positionZ
            };

            // Normal은 모든 Triangle이 생성된 뒤 CalculateNormals에서 계산한다.
            vertex.Normal =
            {
                0.0f,
                0.0f,
                0.0f
            };

            vertex.UV =
            {
                u,
                v
            };

            // 현재 Terrain은 Wireframe으로 확인하는 단계이므로
            // 선이 어두운 배경에서 잘 보이는 밝은 색을 사용한다.
            vertex.Color =
            {
                0.72f,
                0.82f,
                0.95f,
                1.0f
            };

            outMeshData.Vertices.push_back(
                vertex);
        }
    }

    // 한 Row에 들어가는 Vertex 개수.
    const std::uint32_t rowStride =
        settings.CellsX + 1u;

    // ------------------------------------------------------------------------
    // Index 생성
    //
    // 한 Cell을 Triangle 2개로 분할한다.
    //
    //   topLeft ------- topRight
    //      |          /     |
    //      |        /       |
    //      |      /         |
    //   bottomLeft ---- bottomRight
    //
    // Index Winding은 Triangle의 기하학적 Normal이 +Y를 향하도록 만든다.
    // 기존 Renderer의 D3D11_CULL_BACK 설정과 호환되기 위한 부분이다.
    // ------------------------------------------------------------------------
    for (std::uint32_t z = 0u;
         z < settings.CellsZ;
         ++z)
    {
        for (std::uint32_t x = 0u;
             x < settings.CellsX;
             ++x)
        {
            const std::uint32_t topLeft =
                z * rowStride +
                x;

            const std::uint32_t topRight =
                topLeft + 1u;

            const std::uint32_t bottomLeft =
                (z + 1u) * rowStride +
                x;

            const std::uint32_t bottomRight =
                bottomLeft + 1u;

            // Triangle 1
            outMeshData.Indices.push_back(
                topLeft);

            outMeshData.Indices.push_back(
                bottomLeft);

            outMeshData.Indices.push_back(
                topRight);

            // Triangle 2
            outMeshData.Indices.push_back(
                topRight);

            outMeshData.Indices.push_back(
                bottomLeft);

            outMeshData.Indices.push_back(
                bottomRight);
        }
    }

    // Height가 적용된 실제 Triangle Geometry를 기준으로
    // 향후 Lighting에 사용할 Vertex Normal까지 미리 계산한다.
    CalculateNormals(
        outMeshData.Vertices,
        outMeshData.Indices);

    return true;
}

void PerlinTerrainGenerator::CalculateNormals(
    std::vector<Vertex>& vertices,
    const std::vector<std::uint32_t>& indices)
{
    // 혹시 재호출되더라도 이전 Normal이 누적되지 않도록 초기화한다.
    for (Vertex& vertex : vertices)
    {
        vertex.Normal =
        {
            0.0f,
            0.0f,
            0.0f
        };
    }

    // ------------------------------------------------------------------------
    // Triangle 단위 Face Normal 계산
    //
    // Normalize하지 않은 Cross Product를 누적한다.
    // 그러면 넓은 Triangle이 조금 더 큰 가중치를 가지므로
    // 일반적인 Area Weighted Vertex Normal이 된다.
    // ------------------------------------------------------------------------
    for (std::size_t index = 0;
         index + 2u < indices.size();
         index += 3u)
    {
        const std::uint32_t index0 =
            indices[index + 0u];

        const std::uint32_t index1 =
            indices[index + 1u];

        const std::uint32_t index2 =
            indices[index + 2u];

        const XMVECTOR position0 =
            XMLoadFloat3(
                &vertices[index0].Position);

        const XMVECTOR position1 =
            XMLoadFloat3(
                &vertices[index1].Position);

        const XMVECTOR position2 =
            XMLoadFloat3(
                &vertices[index2].Position);

        const XMVECTOR edge01 =
            XMVectorSubtract(
                position1,
                position0);

        const XMVECTOR edge02 =
            XMVectorSubtract(
                position2,
                position0);

        const XMVECTOR faceNormal =
            XMVector3Cross(
                edge01,
                edge02);

        XMFLOAT3 faceNormalFloat = {};

        XMStoreFloat3(
            &faceNormalFloat,
            faceNormal);

        auto accumulateNormal =
            [&faceNormalFloat](Vertex& vertex)
        {
            vertex.Normal.x +=
                faceNormalFloat.x;

            vertex.Normal.y +=
                faceNormalFloat.y;

            vertex.Normal.z +=
                faceNormalFloat.z;
        };

        accumulateNormal(
            vertices[index0]);

        accumulateNormal(
            vertices[index1]);

        accumulateNormal(
            vertices[index2]);
    }

    // ------------------------------------------------------------------------
    // 누적된 Normal을 최종적으로 정규화한다.
    // ------------------------------------------------------------------------
    constexpr float normalEpsilon =
        0.000001f;

    for (Vertex& vertex : vertices)
    {
        XMVECTOR normal =
            XMLoadFloat3(
                &vertex.Normal);

        const float lengthSquared =
            XMVectorGetX(
                XMVector3LengthSq(normal));

        if (lengthSquared <= normalEpsilon)
        {
            // Degenerate Triangle 등으로 Normal을 구하지 못한 경우
            // Terrain 기본 Up Vector를 사용한다.
            vertex.Normal =
            {
                0.0f,
                1.0f,
                0.0f
            };

            continue;
        }

        normal =
            XMVector3Normalize(normal);

        XMStoreFloat3(
            &vertex.Normal,
            normal);
    }
}
