#include "Features/TerrainPlacement/TerrainPlacementFeature.h"
#include "Terrain/Query/TerrainPlacement.h"
#include "Features/CollisionEditor/CollisionScene.h"
#include "Graphics/Camera.h"
#include "Input/Input.h"
#include "UI/UIManager.h"
using namespace DirectX;

void TerrainPlacementFeature::Attach(UIManager& ui,CollisionScene& scene)
{
    const auto panel=ui.CreatePanel(L"Terrain 지면 배치",L"지면 배치 열기",L"지면 배치 닫기",490.f,350.f);
    ui.AddToPanel(panel,std::make_unique<UILabel>([this]{return status_;}));
    ui.AddToPanel(panel,std::make_unique<UICheckBox>(L"선택 Collider 지면 배치 모드",
        [this]{return active_;},[this,&scene](bool v){active_=v;scene.InvalidatePicking();}));
    ui.AddToPanel(panel,std::make_unique<UICheckBox>(L"Collider 바닥 기준 배치",
        [this]{return settings_.RestBottom;},[this](bool v){settings_.RestBottom=v;}));
    ui.AddToPanel(panel,std::make_unique<UISlider>(L"지면과 간격",settings_.MinClearance,settings_.MaxClearance,0.01f,
        [this]{return settings_.Clearance;},[this](float v){settings_.Clearance=v;}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"현재 X/Z에서 지면에 놓기",[this,&scene]
    {
        if(const auto* c=scene.Selected())ReportAndPlace(query_.HeightAndNormal(c->Position.x,c->Position.z),scene);
        else status_=L"먼저 Collider를 선택하세요.";
    }));
    ui.AddToPanel(panel,std::make_unique<UILabel>(L"Escape: 모드 해제 / 배치 중 Camera Orbit 차단"));
}
void TerrainPlacementFeature::ReportAndPlace(const TerrainQueryResult& hit,CollisionScene& scene)
{
    auto* collider=scene.Selected();
    if(!collider){status_=L"먼저 Collider를 선택하세요.";return;}
    if(PlaceColliderOnTerrain(*collider,hit,settings_.Clearance,settings_.RestBottom))
    {scene.InvalidatePicking();status_=L"지면 배치 완료 (원본 HeightField 기준)";}
    else if(hit.Status==TerrainQueryStatus::Unavailable)status_=L"Tile 조회 불가: "+hit.Error;
    else if(hit.Status==TerrainQueryStatus::Invalid)status_=L"Terrain Query 입력 오류";
    else status_=L"클릭한 방향에 지형이 없습니다.";
}
bool TerrainPlacementFeature::Update(const Input& input,bool pointerAvailable,const Camera& camera,unsigned width,unsigned height,CollisionScene& scene)
{
    if(!active_)return false;
    if(input.IsKeyPressed(VK_ESCAPE)){active_=false;return true;}
    if(!pointerAvailable||!input.IsLeftMousePressed())return true;
    const int x=input.GetMouseX(),y=input.GetMouseY();
    if(!width||!height||x<0||y<0||x>=static_cast<int>(width)||y>=static_cast<int>(height))return true;
    // Same pixel-center convention as GPU Picking. Camera matrices remain in the
    // platform adapter; TerrainQuery uses only portable world-space vectors.
    const XMMATRIX inverse=XMMatrixInverse(nullptr,camera.GetViewMatrix()*camera.GetProjectionMatrix());
    const float nx=2.f*(x+.5f)/width-1.f,ny=1.f-2.f*(y+.5f)/height;
    XMFLOAT3 a,b;
    XMStoreFloat3(&a,XMVector3TransformCoord(XMVectorSet(nx,ny,0,1),inverse));
    XMStoreFloat3(&b,XMVector3TransformCoord(XMVectorSet(nx,ny,1,1),inverse));
    const Collision::Ray ray{{a.x,a.y,a.z},{b.x-a.x,b.y-a.y,b.z-a.z}};
    ReportAndPlace(query_.Raycast(ray,settings_.MaxRayDistance),scene);return true;
}
