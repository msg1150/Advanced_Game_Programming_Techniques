// ============================================================================
// Raycast.cpp
// ============================================================================
#include "Physics/Query/Raycast.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

namespace Collision
{
namespace
{
    RaycastHit Miss(){return {};}
    RaycastHit MakeHit(ColliderId id,float t,const Vec3& origin,const Vec3& direction,const Vec3& normal)
    {
        RaycastHit hit;
        hit.Hit=true;hit.Collider=id;hit.Distance=t;
        hit.Point=origin+direction*t;hit.Normal=Normalize(normal,{0.f,1.f,0.f});
        return hit;
    }

    RaycastHit RaySphere(const Ray& ray,const ColliderComponent& c,const SphereShape& shape,float maxDistance)
    {
        const Vec3 d=Normalize(ray.Direction,{0.f,0.f,1.f});
        const auto sphere=ToWorldSphere(c,shape);
        const Vec3 m=ray.Origin-sphere.Center;
        const float b=Dot(m,d);
        const float cc=Dot(m,m)-sphere.Radius*sphere.Radius;
        if(cc>0.f&&b>0.f)return Miss();
        const float discr=b*b-cc;
        if(discr<0.f)return Miss();
        float t=-b-std::sqrt(discr);
        if(t<0.f)t=0.f;
        if(t>maxDistance)return Miss();
        const Vec3 p=ray.Origin+d*t;
        return MakeHit(c.Id,t,ray.Origin,d,p-sphere.Center);
    }

    bool Slab(const Vec3& origin,const Vec3& direction,const Vec3& e,float maxDistance,
              float& outT,Vec3& outNormal)
    {
        float tMin=0.f,tMax=maxDistance;
        Vec3 normal{};
        const float o[3]={origin.x,origin.y,origin.z};
        const float d[3]={direction.x,direction.y,direction.z};
        const float ex[3]={e.x,e.y,e.z};
        for(int axis=0;axis<3;++axis)
        {
            if(std::abs(d[axis])<kEpsilon)
            {
                if(o[axis]<-ex[axis]||o[axis]>ex[axis])return false;
                continue;
            }
            float t1=(-ex[axis]-o[axis])/d[axis];
            float t2=( ex[axis]-o[axis])/d[axis];
            float sign=-1.f;
            if(t1>t2){std::swap(t1,t2);sign=1.f;}
            if(t1>tMin)
            {
                tMin=t1;
                normal={0.f,0.f,0.f};
                if(axis==0)normal.x=sign;
                if(axis==1)normal.y=sign;
                if(axis==2)normal.z=sign;
            }
            tMax=std::min(tMax,t2);
            if(tMin>tMax)return false;
        }
        outT=tMin;
        outNormal=normal;
        return tMin<=maxDistance;
    }

    RaycastHit RayOBB(const Ray& ray,const ColliderComponent& c,const WorldOBB& box,float maxDistance)
    {
        const Vec3 worldD=Normalize(ray.Direction,{0.f,0.f,1.f});
        const Vec3 o=WorldToLocal(ray.Origin,box.Center,box.Basis);
        const Vec3 d={Dot(worldD,box.Basis.X),Dot(worldD,box.Basis.Y),Dot(worldD,box.Basis.Z)};
        float t=0.f;Vec3 localNormal{};
        if(!Slab(o,d,box.HalfExtents,maxDistance,t,localNormal))return Miss();
        const Vec3 worldNormal=box.Basis.X*localNormal.x+box.Basis.Y*localNormal.y+box.Basis.Z*localNormal.z;
        return MakeHit(c.Id,t,ray.Origin,worldD,worldNormal);
    }

    RaycastHit RayCapsule(const Ray& ray,const ColliderComponent& c,const CapsuleShape& shape,float maxDistance)
    {
        const Vec3 rd=Normalize(ray.Direction,{0.f,0.f,1.f});
        const auto cap=ToWorldCapsule(c,shape);
        const Vec3 ba=cap.B-cap.A;
        const Vec3 oa=ray.Origin-cap.A;
        const float baba=Dot(ba,ba);

        // Capsule 축 길이가 0이면 Sphere와 동일하다.
        if(baba<=kEpsilon)
        {
            ColliderComponent temp=c;
            temp.Position=cap.A;
            return RaySphere(ray,temp,SphereShape{cap.Radius},maxDistance);
        }

        const float bard=Dot(ba,rd);
        const float baoa=Dot(ba,oa);
        const float rdoa=Dot(rd,oa);
        const float oaoa=Dot(oa,oa);
        const float a=baba-bard*bard;
        const float b=baba*rdoa-baoa*bard;
        const float cc=baba*oaoa-baoa*baoa-cap.Radius*cap.Radius*baba;
        float best=std::numeric_limits<float>::max();
        Vec3 bestNormal{};

        if(std::abs(a)>kEpsilon)
        {
            const float h=b*b-a*cc;
            if(h>=0.f)
            {
                const float t=(-b-std::sqrt(h))/a;
                const float y=baoa+t*bard;
                if(t>=0.f&&t<=maxDistance&&y>0.f&&y<baba)
                {
                    const Vec3 p=ray.Origin+rd*t;
                    const Vec3 axisPoint=cap.A+ba*(y/baba);
                    best=t;bestNormal=p-axisPoint;
                }
            }
        }

        auto testSphere=[&](const Vec3& center)
        {
            const Vec3 m=ray.Origin-center;
            const float qb=Dot(m,rd);
            const float qc=Dot(m,m)-cap.Radius*cap.Radius;
            const float h=qb*qb-qc;
            if(h<0.f)return;
            float t=-qb-std::sqrt(h);
            if(t<0.f)t=-qb+std::sqrt(h);
            if(t>=0.f&&t<best&&t<=maxDistance)
            {
                best=t;bestNormal=(ray.Origin+rd*t)-center;
            }
        };
        testSphere(cap.A);testSphere(cap.B);
        if(best==std::numeric_limits<float>::max())return Miss();
        return MakeHit(c.Id,best,ray.Origin,rd,bestNormal);
    }

    RaycastHit TestShape(const Ray& ray,const ColliderComponent& c,const SphereShape& s,float maxDistance){return RaySphere(ray,c,s,maxDistance);}
    RaycastHit TestShape(const Ray& ray,const ColliderComponent& c,const AABBShape& s,float maxDistance){return RayOBB(ray,c,AABBAsOBB(ToWorldAABB(c,s)),maxDistance);}
    RaycastHit TestShape(const Ray& ray,const ColliderComponent& c,const OBBShape& s,float maxDistance){return RayOBB(ray,c,ToWorldOBB(c,s),maxDistance);}
    RaycastHit TestShape(const Ray& ray,const ColliderComponent& c,const CapsuleShape& s,float maxDistance){return RayCapsule(ray,c,s,maxDistance);}
}

RaycastHit Raycast::Test(const Ray& ray,const ColliderComponent& collider,float maxDistance)
{
    if(!collider.Enabled||maxDistance<0.f)return {};
    return std::visit([&](const auto& shape){return TestShape(ray,collider,shape,maxDistance);},collider.Shape);
}

RaycastHit Raycast::Closest(const Ray& ray,const std::vector<ColliderComponent>& colliders,
                            LayerMask mask,float maxDistance)
{
    RaycastHit best{};
    best.Distance=maxDistance;
    for(const auto& collider:colliders)
    {
        if(!collider.Enabled||(collider.Layer&mask)==0u)continue;
        const auto hit=Test(ray,collider,best.Distance);
        if(hit.Hit&&hit.Distance<=best.Distance)best=hit;
    }
    return best;
}
}
