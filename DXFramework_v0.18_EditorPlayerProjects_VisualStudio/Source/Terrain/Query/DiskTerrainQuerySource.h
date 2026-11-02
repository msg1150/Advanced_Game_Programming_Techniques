#pragma once
#include "Terrain/Query/TerrainQuery.h"
#include "Features/DiskTerrainStreaming/TileSampleCache.h"

// Editor/on-demand provider. Reads only when a query is requested, using its own
// bounded cache. No contention with the render streaming worker or GPU residency.
// A gameplay provider may implement ITerrainTileSource with resident-only reads.
class DiskTerrainQuerySource final:public ITerrainTileSource
{
public:
    bool Initialize(const std::filesystem::path& directory,std::size_t cacheBytes,std::wstring& error);
    const TileWorldInfo& World()const override{return world_;}
    bool Fetch(std::uint32_t x,std::uint32_t z,TileSamples& output,std::wstring& error)override;
private:
    std::filesystem::path directory_;
    TileWorldInfo world_;
    TileSampleCache cache_;
};
