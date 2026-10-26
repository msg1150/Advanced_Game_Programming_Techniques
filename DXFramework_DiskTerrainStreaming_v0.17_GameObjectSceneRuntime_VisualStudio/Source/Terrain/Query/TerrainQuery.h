#pragma once
#include "Features/DiskTerrainStreaming/TileArchive.h"
#include "Physics/Query/Raycast.h"

// Source decides residency/loading policy. Query algorithms never load files,
// create GPU buffers, modify the streaming manager or depend on render LOD.
class ITerrainTileSource
{
public:
    virtual ~ITerrainTileSource()=default;
    virtual const TileWorldInfo& World()const=0;
    virtual bool Fetch(std::uint32_t x,std::uint32_t z,TileSamples& output,std::wstring& error)=0;
};

enum class TerrainQueryStatus {Hit,Miss,Unavailable,Invalid};
struct TerrainQueryResult
{
    TerrainQueryStatus Status=TerrainQueryStatus::Miss;
    Collision::Vec3 Point{},Normal{0,1,0};
    float Distance=0;
    std::wstring Error;
};

class TerrainQuery
{
public:
    explicit TerrainQuery(ITerrainTileSource& source):source_(source){}
    TerrainQueryResult HeightAndNormal(float x,float z);
    TerrainQueryResult Raycast(const Collision::Ray& ray,float maxDistance);
private:
    ITerrainTileSource& source_;
};
