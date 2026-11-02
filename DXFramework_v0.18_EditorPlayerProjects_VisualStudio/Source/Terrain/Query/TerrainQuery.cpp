#include "Terrain/Query/TerrainQuery.h"
#include <cmath>
#include <limits>

using namespace Collision;
namespace
{
    bool Finite(const Vec3& p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
    bool Valid(const TileWorldInfo& w)
    {
        return w.TotalCellsX>0&&w.TotalCellsZ>0&&w.CellsPerTile>0&&
            w.TotalCellsX<static_cast<unsigned>(std::numeric_limits<int>::max())&&
            w.TotalCellsZ<static_cast<unsigned>(std::numeric_limits<int>::max())&&
            w.CellsPerTile<static_cast<unsigned>(std::numeric_limits<int>::max())&&
            w.TilesX==(w.TotalCellsX-1)/w.CellsPerTile+1&&w.TilesZ==(w.TotalCellsZ-1)/w.CellsPerTile+1&&
            std::isfinite(w.CellSize)&&w.CellSize>0&&std::isfinite(w.OriginX)&&std::isfinite(w.OriginZ)&&
            std::isfinite(w.OriginX+w.TotalCellsX*w.CellSize)&&std::isfinite(w.OriginZ+w.TotalCellsZ*w.CellSize);
    }
    bool CellVertices(const TileWorldInfo& w,const TileSamples& tile,int x,int z,Vec3 (&v)[4])
    {
        const auto tx=static_cast<std::uint32_t>(x)/w.CellsPerTile,tz=static_cast<std::uint32_t>(z)/w.CellsPerTile;
        if(tile.TileX!=tx||tile.TileZ!=tz||tile.CellsX!=std::min(w.CellsPerTile,w.TotalCellsX-tx*w.CellsPerTile)||
           tile.CellsZ!=std::min(w.CellsPerTile,w.TotalCellsZ-tz*w.CellsPerTile)||
           tile.Heights.size()!=(static_cast<std::size_t>(tile.CellsX)+3)*(static_cast<std::size_t>(tile.CellsZ)+3))return false;
        const auto lx=static_cast<unsigned>(x)-tx*w.CellsPerTile+1,lz=static_cast<unsigned>(z)-tz*w.CellsPerTile+1;
        const std::size_t stride=static_cast<std::size_t>(tile.CellsX)+3;
        const float px=w.OriginX+x*w.CellSize,pz=w.OriginZ+z*w.CellSize;
        v[0]={px,tile.Heights[lz*stride+lx],pz};
        v[1]={px,tile.Heights[(lz+1)*stride+lx],pz+w.CellSize};
        v[2]={px+w.CellSize,tile.Heights[lz*stride+lx+1],pz};
        v[3]={px+w.CellSize,tile.Heights[(lz+1)*stride+lx+1],pz+w.CellSize};
        return Finite(v[0])&&Finite(v[1])&&Finite(v[2])&&Finite(v[3]);
    }
    bool Triangle(const Ray& ray,const Vec3& a,const Vec3& b,const Vec3& c,float& distance,Vec3& normal)
    {
        const Vec3 e1=b-a,e2=c-a,p=Cross(ray.Direction,e2);
        const float det=Dot(e1,p);
        if(std::abs(det)<1.e-8f)return false;
        const float inv=1.f/det;const Vec3 t=ray.Origin-a;
        const float u=Dot(t,p)*inv;if(u<-1.e-5f||u>1.00001f)return false;
        const Vec3 q=Cross(t,e1);const float v=Dot(ray.Direction,q)*inv;
        if(v<-1.e-5f||u+v>1.00001f)return false;
        distance=Dot(e2,q)*inv;normal=Normalize(Cross(e1,e2),{0,1,0});
        if(normal.y<0)normal=-normal;
        return std::isfinite(distance)&&distance>=-1.e-5f;
    }
    bool Slab(float origin,float direction,float lo,float hi,float& enter,float& exit)
    {
        if(std::abs(direction)<1.e-12f)return origin>=lo&&origin<=hi;
        float a=(lo-origin)/direction,b=(hi-origin)/direction;
        if(a>b)std::swap(a,b);enter=std::max(enter,a);exit=std::min(exit,b);return enter<=exit;
    }
    TerrainQueryResult Unavailable(std::wstring error)
    {TerrainQueryResult r;r.Status=TerrainQueryStatus::Unavailable;r.Error=std::move(error);return r;}
}

TerrainQueryResult TerrainQuery::HeightAndNormal(float x,float z)
{
    const auto& w=source_.World();TerrainQueryResult result;
    if(!Valid(w)||!std::isfinite(x)||!std::isfinite(z)){result.Status=TerrainQueryStatus::Invalid;return result;}
    const float gx=(x-w.OriginX)/w.CellSize,gz=(z-w.OriginZ)/w.CellSize;
    if(gx<0||gz<0||gx>w.TotalCellsX||gz>w.TotalCellsZ)return result;
    const int cx=std::min(static_cast<int>(std::floor(gx)),static_cast<int>(w.TotalCellsX)-1);
    const int cz=std::min(static_cast<int>(std::floor(gz)),static_cast<int>(w.TotalCellsZ)-1);
    TileSamples tile;std::wstring error;Vec3 v[4];
    if(!source_.Fetch(cx/w.CellsPerTile,cz/w.CellsPerTile,tile,error))return Unavailable(error);
    if(!CellVertices(w,tile,cx,cz,v))return Unavailable(L"Invalid terrain tile samples.");
    const float u=gx-cx,t=gz-cz;
    float h;
    if(u+t<=1){h=v[0].y+(v[2].y-v[0].y)*u+(v[1].y-v[0].y)*t;result.Normal=Normalize(Cross(v[1]-v[0],v[2]-v[0]),{0,1,0});}
    else {h=v[3].y+(v[1].y-v[3].y)*(1-u)+(v[2].y-v[3].y)*(1-t);result.Normal=Normalize(Cross(v[1]-v[2],v[3]-v[2]),{0,1,0});}
    result.Status=TerrainQueryStatus::Hit;result.Point={x,h,z};return result;
}

TerrainQueryResult TerrainQuery::Raycast(const Ray& input,float maxDistance)
{
    const auto& w=source_.World();TerrainQueryResult result;
    const float length=Length(input.Direction);
    if(!Valid(w)||!Finite(input.Origin)||!Finite(input.Direction)||!std::isfinite(length)||length<kEpsilon||
       !std::isfinite(maxDistance)||maxDistance<0)
    {result.Status=TerrainQueryStatus::Invalid;return result;}
    const Ray ray{input.Origin,input.Direction/length};float enter=0,exit=maxDistance;
    if(!Slab(ray.Origin.x,ray.Direction.x,w.OriginX,w.OriginX+w.TotalCellsX*w.CellSize,enter,exit)||
       !Slab(ray.Origin.z,ray.Direction.z,w.OriginZ,w.OriginZ+w.TotalCellsZ*w.CellSize,enter,exit))return result;
    const Vec3 start=ray.Origin+ray.Direction*enter;
    int x=std::clamp(static_cast<int>(std::floor((start.x-w.OriginX)/w.CellSize)),0,static_cast<int>(w.TotalCellsX)-1);
    int z=std::clamp(static_cast<int>(std::floor((start.z-w.OriginZ)/w.CellSize)),0,static_cast<int>(w.TotalCellsZ)-1);
    const int sx=ray.Direction.x>0?1:ray.Direction.x<0?-1:0,sz=ray.Direction.z>0?1:ray.Direction.z<0?-1:0;
    const float inf=std::numeric_limits<float>::infinity();
    const float dx=sx?w.CellSize/std::abs(ray.Direction.x):inf,dz=sz?w.CellSize/std::abs(ray.Direction.z):inf;
    float nextX=sx?(w.OriginX+(x+(sx>0?1:0))*w.CellSize-ray.Origin.x)/ray.Direction.x:inf;
    float nextZ=sz?(w.OriginZ+(z+(sz>0?1:0))*w.CellSize-ray.Origin.z)/ray.Direction.z:inf;
    TileSamples tile;int loadedX=-1,loadedZ=-1;float current=enter;
    // 2D grid DDA visits at most CellsX+CellsZ+1 cells, not the entire world.
    const std::uint64_t limit=static_cast<std::uint64_t>(w.TotalCellsX)+w.TotalCellsZ+2;
    for(std::uint64_t step=0;step<limit&&current<=exit+1.e-5f;++step)
    {
        if(x<0||z<0||x>=static_cast<int>(w.TotalCellsX)||z>=static_cast<int>(w.TotalCellsZ))break;
        const int tx=x/static_cast<int>(w.CellsPerTile),tz=z/static_cast<int>(w.CellsPerTile);
        if(tx!=loadedX||tz!=loadedZ)
        {
            std::wstring error;
            if(!source_.Fetch(tx,tz,tile,error))return Unavailable(error);
            loadedX=tx;loadedZ=tz;
        }
        Vec3 v[4];if(!CellVertices(w,tile,x,z,v))return Unavailable(L"Invalid terrain tile samples.");
        const float end=std::min({nextX,nextZ,exit});float nearest=inf;Vec3 bestNormal;
        for(int tri=0;tri<2;++tri)
        {
            float distance;Vec3 normal;
            const bool hit=tri==0?Triangle(ray,v[0],v[1],v[2],distance,normal):Triangle(ray,v[2],v[1],v[3],distance,normal);
            if(hit&&distance>=current-1.e-4f&&distance<=end+1.e-4f&&distance<=maxDistance&&distance<nearest)
            {nearest=std::max(0.f,distance);bestNormal=normal;}
        }
        if(std::isfinite(nearest))
        {result.Status=TerrainQueryStatus::Hit;result.Distance=nearest;result.Point=ray.Origin+ray.Direction*nearest;result.Normal=bestNormal;return result;}
        if(end>=exit||(!sx&&!sz))break;
        current=end;
        if(nextX<nextZ){x+=sx;nextX+=dx;}
        else if(nextZ<nextX){z+=sz;nextZ+=dz;}
        else {x+=sx;z+=sz;nextX+=dx;nextZ+=dz;}
    }
    return result;
}
