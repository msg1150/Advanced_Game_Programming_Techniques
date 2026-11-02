#include "Terrain/Query/DiskTerrainQuerySource.h"

bool DiskTerrainQuerySource::Initialize(const std::filesystem::path& directory,std::size_t cacheBytes,std::wstring& error)
{
    TileWorldInfo candidate;
    if(!TileArchive::ReadMetadata(directory/L"Terrain.meta",candidate,error))return false;
    directory_=directory;world_=std::move(candidate);cache_.Clear();cache_.SetCapacity(cacheBytes);return true;
}
bool DiskTerrainQuerySource::Fetch(std::uint32_t x,std::uint32_t z,TileSamples& output,std::wstring& error)
{
    error.clear();
    if(x>=world_.TilesX||z>=world_.TilesZ){error=L"Terrain query tile out of range.";return false;}
    const auto index=z*world_.TilesX+x;
    if(cache_.Get(index,output))return true;
    if(!TileArchive::ReadTile(directory_,world_,x,z,output,error))return false;
    cache_.Put(index,output);return true;
}
