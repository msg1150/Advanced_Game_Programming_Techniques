// ============================================================================
// TerrainChunkGenerator.h
// 전체 HeightMap Mesh에서 정규 Grid 경계가 겹치는 독립 Chunk Vertex를 추출한다.
// GPU, 카메라, 재질에 대해 전혀 알지 못하는 CPU 데이터 생성기이다.
// ============================================================================
#pragma once
#include "Features/HeightMapTerrain/HeightMapTerrainGenerator.h"
#include <cstdint>
#include <string>
#include <vector>

struct TerrainChunkMeshData
{
    std::uint32_t ChunkX = 0u;
    std::uint32_t ChunkZ = 0u;
    std::uint32_t StartCellX = 0u;
    std::uint32_t StartCellZ = 0u;
    std::uint32_t CellsX = 0u;
    std::uint32_t CellsZ = 0u;
    std::uint32_t VertexWidth = 0u;
    std::uint32_t VertexHeight = 0u;
    std::vector<Vertex> Vertices;
};

class TerrainChunkGenerator
{
public:
    static bool Generate(
        const HeightMapTerrainMeshData& fullMesh,
        std::uint32_t imageWidth,
        std::uint32_t imageHeight,
        std::uint32_t cellsPerChunk,
        std::vector<TerrainChunkMeshData>& outChunks,
        std::wstring& outError);
};
