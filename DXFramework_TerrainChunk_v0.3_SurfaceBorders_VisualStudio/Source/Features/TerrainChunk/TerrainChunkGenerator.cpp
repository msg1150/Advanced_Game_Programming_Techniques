// ============================================================================
// TerrainChunkGenerator.cpp
// 핵심: 전체 지형에서 Normal을 계산한 다음 Chunk별로 그대로 복사한다.
// 인접 Chunk가 동일한 경계 Pixel의 높이, UV, Normal을 가지게 된다.
// 좌표도 동일한 전체 지형 Local 좌표로 유지해 경계 위치가 일치한다.
// ============================================================================
#include "Features/TerrainChunk/TerrainChunkGenerator.h"
#include <algorithm>
#include <limits>
#include <utility>

bool TerrainChunkGenerator::Generate(
    const HeightMapTerrainMeshData& fullMesh,
    std::uint32_t imageWidth,
    std::uint32_t imageHeight,
    std::uint32_t cellsPerChunk,
    std::vector<TerrainChunkMeshData>& outChunks,
    std::wstring& outError)
{
    outChunks.clear();
    outError.clear();

    if (imageWidth < 2u || imageHeight < 2u || cellsPerChunk == 0u)
    {
        outError = L"Chunk 입력 오류: HeightMap은 최소 2x2, CellsPerChunk는 1 이상이어야 합니다.";
        return false;
    }
    const std::uint64_t requiredCount =
        static_cast<std::uint64_t>(imageWidth) * imageHeight;
    if (fullMesh.Vertices.size() != requiredCount)
    {
        outError = L"전체 HeightMap의 Vertex 개수와 이미지 크기가 일치하지 않습니다.";
        return false;
    }
    const std::uint32_t totalCellsX = imageWidth - 1u;
    const std::uint32_t totalCellsZ = imageHeight - 1u;
    // 올림 나눗셈. 마지막 Chunk가 32칸보다 작아도 누락되지 않게 처리한다.
    const std::uint64_t countX = 1ull + (totalCellsX - 1ull) / cellsPerChunk;
    const std::uint64_t countZ = 1ull + (totalCellsZ - 1ull) / cellsPerChunk;
    if (countX * countZ > std::numeric_limits<std::uint32_t>::max())
    {
        outError = L"Chunk 개수가 uint32 범위를 초과했습니다.";
        return false;
    }
    outChunks.reserve(static_cast<std::size_t>(countX * countZ));

    for (std::uint32_t z = 0u; z < countZ; ++z)
    {
        for (std::uint32_t x = 0u; x < countX; ++x)
        {
            TerrainChunkMeshData data;
            data.ChunkX = x;
            data.ChunkZ = z;
            data.StartCellX = x * cellsPerChunk;
            data.StartCellZ = z * cellsPerChunk;
            data.CellsX = std::min(cellsPerChunk, totalCellsX - data.StartCellX);
            data.CellsZ = std::min(cellsPerChunk, totalCellsZ - data.StartCellZ);
            data.VertexWidth = data.CellsX + 1u;
            data.VertexHeight = data.CellsZ + 1u;
            data.Vertices.reserve(static_cast<std::size_t>(data.VertexWidth) * data.VertexHeight);

            // 각 Chunk가 경계 Vertex를 함께 포함한다(+1).
            // 전역 Vertex 값을 재계산하지 않고 복제하므로 경계의 Y/UV/Normal이 같다.
            for (std::uint32_t localZ = 0u; localZ < data.VertexHeight; ++localZ)
            {
                for (std::uint32_t localX = 0u; localX < data.VertexWidth; ++localX)
                {
                    const std::size_t globalIndex =
                        static_cast<std::size_t>(data.StartCellZ + localZ) * imageWidth +
                        data.StartCellX + localX;
                    data.Vertices.push_back(fullMesh.Vertices[globalIndex]);
                }
            }
            outChunks.push_back(std::move(data));
        }
    }
    return true;
}
