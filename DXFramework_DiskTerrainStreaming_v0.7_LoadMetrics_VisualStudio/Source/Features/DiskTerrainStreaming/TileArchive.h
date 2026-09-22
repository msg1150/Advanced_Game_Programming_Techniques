// ============================================================================
// TileArchive.h — Windows/Direct3D와 독립적인 디스크 Tile 포맷 처리 계층
// 메타데이터만 초기 로드하며, 실제 높이 Sample은 요청된 Tile 파일만 읽는다.
// ============================================================================
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct TileInfo
{
    float MinimumHeight = 0.0f;
    float MaximumHeight = 0.0f;
};
struct TileWorldInfo
{
    std::uint32_t TotalCellsX = 0, TotalCellsZ = 0, CellsPerTile = 0;
    std::uint32_t TilesX = 0, TilesZ = 0;
    float CellSize = 0.0f, OriginX = 0.0f, OriginZ = 0.0f;
    std::vector<TileInfo> Tiles;
};
struct TileSamples
{
    std::uint32_t TileX = 0, TileZ = 0, CellsX = 0, CellsZ = 0;
    // 1-pixel Halo: (CellsX+3) * (CellsZ+3) absolute world heights.
    std::vector<float> Heights;
};
class TileArchive
{
public:
    static bool ReadMetadata(const std::filesystem::path& path,
                             TileWorldInfo& output, std::wstring& error);
    static bool ReadTile(const std::filesystem::path& root,
                         const TileWorldInfo& meta, std::uint32_t x, std::uint32_t z,
                         TileSamples& output, std::wstring& error);
    static std::filesystem::path TileFile(const std::filesystem::path& directory,
                                          std::uint32_t x, std::uint32_t z);
};
