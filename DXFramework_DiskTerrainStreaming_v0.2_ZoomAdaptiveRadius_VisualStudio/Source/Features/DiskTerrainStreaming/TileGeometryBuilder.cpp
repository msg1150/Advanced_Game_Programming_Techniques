// ============================================================================
// TileGeometryBuilder.cpp
// ---------------------------------------------------------------------------
// 파일에 저장된 [-1,+1] Halo를 중앙 차분에 사용한다. 경계 Sample과 양측
// 미분값이 인접 Tile에서 같으므로 높이와 Smooth Normal이 이어진다.
// ============================================================================
#include "Features/DiskTerrainStreaming/TileGeometryBuilder.h"
#include <algorithm>
#include <cmath>
#include <limits>
using namespace DirectX;

bool TileGeometryBuilder::Build(const TileWorldInfo& world,const TileSamples& tile,
                                const QuadTreeLODSettings& lod,TilePrepared& result)
{
    const auto width=tile.CellsX+1u;
    const auto height=tile.CellsZ+1u;
    const auto haloWidth=tile.CellsX+3u;
    if(!width || !height || tile.Heights.size()!=
       static_cast<std::size_t>(haloWidth)*(tile.CellsZ+3u))
    { result.Error=L"Tile Halo Sample 크기 오류";return false; }
    const auto sample=[&](std::uint32_t x,std::uint32_t z)->float
    { return tile.Heights[static_cast<std::size_t>(z)*haloWidth+x]; };
    std::vector<Vertex> vertices;
    vertices.reserve(static_cast<std::size_t>(width)*height);
    const auto baseX=tile.TileX*world.CellsPerTile;
    const auto baseZ=tile.TileZ*world.CellsPerTile;
    for(std::uint32_t z=0;z<height;++z)
    for(std::uint32_t x=0;x<width;++x)
    {
        const float h=sample(x+1u,z+1u);
        const float dhdx=sample(x+2u,z+1u)-sample(x,z+1u);
        const float dhdz=sample(x+1u,z+2u)-sample(x+1u,z);
        const float nx=-dhdx,ny=2.0f*world.CellSize,nz=-dhdz;
        const float len=std::sqrt(nx*nx+ny*ny+nz*nz);
        Vertex v={};
        v.Position={world.OriginX+(baseX+x)*world.CellSize,h,
                    world.OriginZ+(baseZ+z)*world.CellSize};
        v.Normal={nx/len,ny/len,nz/len};
        v.UV={float(baseX+x)/world.TotalCellsX,float(baseZ+z)/world.TotalCellsZ};
        v.Color={1.f,1.f,1.f,1.f};
        vertices.push_back(v);
    }
    if(!result.Tree.Build(vertices,width,height,lod,result.MeshData,result.Error))
        return false;
    // 서로 붙은 Tile의 경계 중복 드로우 방지: 북/서 기본, 마지막 타일만 남/동.
    const bool lastX=tile.TileX+1u==world.TilesX;
    const bool lastZ=tile.TileZ+1u==world.TilesZ;
    const auto add=[&](std::uint32_t x0,std::uint32_t z0,
                       std::uint32_t x1,std::uint32_t z1)
    {
        Vertex a=vertices[static_cast<std::size_t>(z0)*width+x0];
        Vertex b=vertices[static_cast<std::size_t>(z1)*width+x1];
        a.Position.y+=0.04f;b.Position.y+=0.04f;
        a.Color={0.2f,0.85f,1.0f,1.0f};b.Color=a.Color;
        const auto start=static_cast<std::uint32_t>(result.BorderVertices.size());
        result.BorderVertices.push_back(a);result.BorderVertices.push_back(b);
        result.BorderIndices.push_back(start);result.BorderIndices.push_back(start+1u);
    };
    for(std::uint32_t x=0;x<tile.CellsX;++x)add(x,0,x+1,0);
    for(std::uint32_t z=0;z<tile.CellsZ;++z)add(0,z,0,z+1);
    if(lastX)for(std::uint32_t z=0;z<tile.CellsZ;++z)add(tile.CellsX,z,tile.CellsX,z+1);
    if(lastZ)for(std::uint32_t x=0;x<tile.CellsX;++x)add(x,tile.CellsZ,x+1,tile.CellsZ);
    return true;
}
