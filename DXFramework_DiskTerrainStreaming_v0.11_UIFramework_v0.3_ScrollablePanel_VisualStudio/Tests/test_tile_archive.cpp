// Pure C++ parser test. Does not require Windows or Direct3D SDK.
#include "Features/DiskTerrainStreaming/TileArchive.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
int main(int argc,char** argv)
{
    if(argc!=2)return 2;
    const auto dir=std::filesystem::path(argv[1]);
    TileWorldInfo meta;std::wstring error;
    assert(TileArchive::ReadMetadata(dir/"Terrain.meta",meta,error));
    assert(meta.TilesX==16 && meta.TilesZ==16 && meta.CellsPerTile==64);
    assert(meta.TotalCellsX==1024 && meta.TotalCellsZ==1024);
    TileSamples a,b,c;
    assert(TileArchive::ReadTile(dir,meta,7,8,a,error));
    assert(TileArchive::ReadTile(dir,meta,8,8,b,error));
    assert(TileArchive::ReadTile(dir,meta,7,9,c,error));
    assert(a.Heights.size()==67u*67u);
    auto sample=[](const TileSamples& tile,int x,int z)->float
    {return tile.Heights[static_cast<std::size_t>(z)*67u+x];};
    for(int i=0;i<=64;++i)
    {
        // Tile 7's right edge == Tile 8's left edge; halo too.
        assert(sample(a,65,i+1)==sample(b,1,i+1));
        assert(sample(a,64,i+1)==sample(b,0,i+1));
        assert(sample(a,66,i+1)==sample(b,2,i+1));
        // Tile 8's lower edge == Tile 9's upper edge; halo too.
        assert(sample(a,i+1,65)==sample(c,i+1,1));
        assert(sample(a,i+1,64)==sample(c,i+1,0));
        assert(sample(a,i+1,66)==sample(c,i+1,2));
    }
    std::size_t count=0;
    for(std::uint32_t z=0;z<meta.TilesZ;++z)
    for(std::uint32_t x=0;x<meta.TilesX;++x)
    {
        TileSamples tile;
        assert(TileArchive::ReadTile(dir,meta,x,z,tile,error));
        assert(tile.Heights.size()==67u*67u);
        ++count;
    }
    TileSamples invalid;
    assert(!TileArchive::ReadTile(dir,meta,17,0,invalid,error));
    std::cout << "PASS tiles=" << count << " seam heights and halos=PASS invalid index=PASS\n";
}
