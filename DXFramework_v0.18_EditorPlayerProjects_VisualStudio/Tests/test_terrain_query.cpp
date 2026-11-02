#include "Terrain/Query/TerrainPlacement.h"
#include "Terrain/Query/DiskTerrainQuerySource.h"
#include <cassert>
#include <cmath>
#include <limits>

class MemorySource:public ITerrainTileSource
{
public:
    TileWorldInfo world{4,3,2,2,2,2.f,-4.f,-2.f,{}};
    bool missing=false;int fetches=0;
    const TileWorldInfo& World()const override{return world;}
    bool Fetch(std::uint32_t tx,std::uint32_t tz,TileSamples& t,std::wstring& error)override
    {
        ++fetches;if(missing){error=L"unloaded";return false;}
        t={tx,tz,std::min(2u,world.TotalCellsX-tx*2),std::min(2u,world.TotalCellsZ-tz*2),{}};
        for(int z=-1;z<=static_cast<int>(t.CellsZ)+1;++z)
        for(int x=-1;x<=static_cast<int>(t.CellsX)+1;++x)
        {
            const float wx=world.OriginX+(int(tx*2)+x)*world.CellSize;
            const float wz=world.OriginZ+(int(tz*2)+z)*world.CellSize;
            t.Heights.push_back(3.f+.25f*wx+.5f*wz);
        }
        return true;
    }
};
static bool Near(float a,float b){return std::abs(a-b)<.001f;}
int main(int argc,char** argv)
{
    using namespace Collision;
    MemorySource source;TerrainQuery query(source);
    // Both cell triangles, tile seams, final partial tile, negative origins.
    for(const Vec3 point: {Vec3{-3.8f,0,-1.8f},{-2.2f,0,-.2f},{0,0,2},{4,0,4},{-4,0,-2}})
    {
        const auto h=query.HeightAndNormal(point.x,point.z);assert(h.Status==TerrainQueryStatus::Hit);
        assert(Near(h.Point.y,3+.25f*point.x+.5f*point.z)&&h.Normal.y>0);
        const auto ray=query.Raycast({{point.x,30,point.z},{0,-2,0}},100);
        assert(ray.Status==TerrainQueryStatus::Hit&&Near(ray.Point.y,h.Point.y)&&Near(ray.Distance,30-h.Point.y));
    }
    // Cross several cells and tile boundaries, from outside the terrain.
    const auto diagonal=query.Raycast({{-10,10,0},{1,-1,0}},100);assert(diagonal.Status==TerrainQueryStatus::Hit);
    assert(Near(diagonal.Point.y,3+.25f*diagonal.Point.x));
    assert(query.Raycast({{10,10,0},{-1,-1,0}},100).Status==TerrainQueryStatus::Hit);
    assert(query.Raycast({{0,30,0},{0,1,0}},100).Status==TerrainQueryStatus::Miss);
    assert(query.Raycast({{0,30,0},{0,-1,0}},1).Status==TerrainQueryStatus::Miss);
    assert(query.Raycast({{100,30,0},{0,-1,0}},100).Status==TerrainQueryStatus::Miss);
    assert(query.Raycast({{0,0,0},{0,0,0}},100).Status==TerrainQueryStatus::Invalid);
    assert(query.HeightAndNormal(std::numeric_limits<float>::quiet_NaN(),0).Status==TerrainQueryStatus::Invalid);
    source.missing=true;assert(query.HeightAndNormal(0,0).Status==TerrainQueryStatus::Unavailable);
    assert(query.Raycast({{0,30,0},{0,-1,0}},100).Status==TerrainQueryStatus::Unavailable);source.missing=false;
    ColliderComponent collider;collider.Shape=OBBShape{{1,2,3}};collider.Rotation={.2f,.5f,.3f};
    const auto hit=query.HeightAndNormal(1,1);
    assert(PlaceColliderOnTerrain(collider,hit,.1f,true));assert(Near(ComputeWorldBounds(collider).Min.y,hit.Point.y+.1f));
    const auto position=collider.Position;TerrainQueryResult miss;
    assert(!PlaceColliderOnTerrain(collider,miss,0,true)&&collider.Position.y==position.y);
    // Real archive integration reuses existing TileArchive and bounded sample cache.
    if(argc>1)
    {
        DiskTerrainQuerySource disk;std::wstring error;
        assert(disk.Initialize(argv[1],1024*1024,error));TerrainQuery real(disk);
        const auto& w=disk.World();const float x=w.OriginX+w.CellSize*.5f,z=w.OriginZ+w.CellSize*.5f;
        const auto height=real.HeightAndNormal(x,z);assert(height.Status==TerrainQueryStatus::Hit);
        const auto ray=real.Raycast({{x,height.Point.y+100,z},{0,-1,0}},200);
        assert(ray.Status==TerrainQueryStatus::Hit&&Near(ray.Point.y,height.Point.y));
    }
}
