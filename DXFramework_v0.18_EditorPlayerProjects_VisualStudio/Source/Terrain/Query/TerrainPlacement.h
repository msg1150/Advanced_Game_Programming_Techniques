#pragma once
#include "Terrain/Query/TerrainQuery.h"

// Pure placement policy reusable by other editors. Contact is the terrain hit
// below the collider center; this is not a slope collision solver.
inline bool PlaceColliderOnTerrain(Collision::ColliderComponent& collider,const TerrainQueryResult& hit,
                                  float clearance,bool restBottom)
{
    if(hit.Status!=TerrainQueryStatus::Hit||!std::isfinite(clearance))return false;
    auto candidate=collider;candidate.Position=hit.Point;
    if(restBottom)candidate.Position.y+=hit.Point.y-Collision::ComputeWorldBounds(candidate).Min.y;
    candidate.Position.y+=clearance;collider.Position=candidate.Position;return true;
}
