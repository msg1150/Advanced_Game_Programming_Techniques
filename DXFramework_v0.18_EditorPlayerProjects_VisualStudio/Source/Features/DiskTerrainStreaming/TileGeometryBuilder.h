// ============================================================================
// TileGeometryBuilder.h — 디스크 Sample로부터 Tile Mesh, LOD, 경계선을 생성.
// GPU/Device 호출은 없다. 인접 Tile과 높이·Normal을 공유하도록 Halo 사용.
// ============================================================================
#pragma once
#include "Features/DiskTerrainStreaming/TileArchive.h"
#include "Features/QuadTreeLOD/QuadTreeLOD.h"
#include <cstdint>
#include <string>
#include <vector>
struct TilePrepared
{
    std::uint32_t Index=0;
    std::uint64_t Generation=0;
    QuadTreeLOD Tree;
    QuadTreeLODMeshData MeshData;
    std::vector<Vertex> BorderVertices;
    std::vector<std::uint32_t> BorderIndices;
    std::wstring Error;
    std::uint64_t DiskBytes=0;
};
class TileGeometryBuilder
{
public:
    static bool Build(const TileWorldInfo& world,const TileSamples& tile,
                      const QuadTreeLODSettings& lod,TilePrepared& result);
};
