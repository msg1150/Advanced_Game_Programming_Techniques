#pragma once
#include "Terrain/Query/TerrainQuery.h"
class Camera;
class Input;
class UIManager;
class CollisionScene;

struct TerrainPlacementSettings
{
    float MaxRayDistance=1200.f;
    float Clearance=0.02f;
    float MinClearance=-5.f,MaxClearance=10.f;
    bool RestBottom=true;
};

class TerrainPlacementFeature
{
public:
    explicit TerrainPlacementFeature(TerrainQuery& query,TerrainPlacementSettings settings={})
        :query_(query),settings_(settings){}
    void Attach(UIManager& ui,CollisionScene& scene);
    // Returns true while this tool owns scene pointer gestures.
    bool Update(const Input& input,bool pointerAvailable,const Camera& camera,unsigned width,unsigned height,CollisionScene& scene);
private:
    void ReportAndPlace(const TerrainQueryResult& hit,CollisionScene& scene);
    TerrainQuery& query_;
    TerrainPlacementSettings settings_;
    bool active_=false;
    std::wstring status_=L"Collider 선택 → 지면 배치 모드 → 지형 클릭";
};
