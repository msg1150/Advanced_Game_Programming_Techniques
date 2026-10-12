// ============================================================================
// CollisionScene.cpp
// ============================================================================
#include "Features/CollisionEditor/CollisionScene.h"
#include "Features/CollisionEditor/CollisionEditorPanel.h"
#include "Graphics/Camera.h"
#include "Input/Input.h"
#include "UI/UIManager.h"

#include <algorithm>
#include <cwchar>
#include <sstream>
#include <type_traits>

using namespace Collision;
using namespace DirectX;

bool CollisionScene::Initialize(ID3D11Device* device,const std::filesystem::path& shaderRoot)
{
    error_.clear();
    if(!debugRenderer_.Initialize(device,shaderRoot/L"CollisionDebug"))
    {
        error_=L"Collision Debug Renderer 초기화 실패\n"+debugRenderer_.GetError();
        return false;
    }
    if(!gpuPicking_.Initialize(device,shaderRoot/L"GPUPicking"))
    {
        error_=L"GPU Picking 초기화 실패\n"+gpuPicking_.GetError();
        return false;
    }
    return true;
}

void CollisionScene::RegisterUI(UIManager& ui)
{
    CollisionEditorPanel::Register(ui,*this);
}

void CollisionScene::SetSpawnAnchor(const XMFLOAT3& target)
{
    spawnAnchor_={target.x,target.y,target.z};
}

void CollisionScene::UpdateInput(const Input& input,bool scenePointerAvailable)
{
    if(gpuPickingEnabled_&&scenePointerAvailable&&input.IsLeftMousePressed())
        pendingPick_=PendingPick{input.GetMouseX(),input.GetMouseY()};
}

void CollisionScene::UpdateCollisions()
{
    if(selectedId_!=0&&!world_.Find(selectedId_))selectedId_=0;
    world_.UpdateCollisions();
}

void CollisionScene::ProcessGPUPicking(ID3D11DeviceContext* context,ID3D11DepthStencilView* sceneDepth,
                                       const XMMATRIX& view,const XMMATRIX& projection,
                                       UINT width,UINT height)
{
    // 이전 Frame에 제출한 1x1 Readback이 끝났는지 non-blocking Poll.
    if(auto completed=gpuPicking_.Poll(context))
    {
        lastPick_=*completed;
        if(completed->Hit&&world_.Find(completed->ObjectId))selectedId_=completed->ObjectId;
        else selectedId_=0;
    }

    if(!gpuPickingEnabled_||!pendingPick_)return;
    if(gpuPicking_.Submit(context,sceneDepth,view,projection,world_.Colliders(),
                          pendingPick_->X,pendingPick_->Y,width,height))
    {
        pendingPick_.reset();
    }
    // Ring 3개가 모두 Pending이면 요청을 버리지 않고 다음 Frame에 다시 제출한다.
}

void CollisionScene::RenderDebug(ID3D11DeviceContext* context,const XMMATRIX& view,const XMMATRIX& projection)
{
    if(!debugVisible_)return;
    debugRenderer_.Render(context,view,projection,world_,selectedId_,highlightCollisions_,showContacts_);
}

Vec3 CollisionScene::NextSpawnPosition()const
{
    // 같은 위치에 완전히 겹쳐 생성하지 않고, Camera Target 주변 4칸에 순환 배치한다.
    // 기본 HalfExtent=2라 서로 가까이 두면 Collision Highlight도 즉시 확인할 수 있다.
    const std::size_t index=world_.Count();
    const float xOffset=(static_cast<float>(index%4u)-1.5f)*5.f;
    const float zOffset=static_cast<float>((index/4u)%3u)*5.f;
    return {spawnAnchor_.x+xOffset,spawnAnchor_.y+5.f,spawnAnchor_.z+zOffset};
}

void CollisionScene::AddSphere(){selectedId_=world_.AddSphere(NextSpawnPosition(),2.f);}
void CollisionScene::AddAABB(){selectedId_=world_.AddAABB(NextSpawnPosition(),{2.f,2.f,2.f});}
void CollisionScene::AddOBB(){
    selectedId_=world_.AddOBB(NextSpawnPosition(),{2.5f,1.5f,2.f});
    if(auto* c=Selected())c->Rotation.y=DegreesToRadians(25.f);
}
void CollisionScene::AddCapsule(){selectedId_=world_.AddCapsule(NextSpawnPosition(),1.5f,2.f);}

void CollisionScene::DeleteSelected()
{
    if(selectedId_==0)return;
    world_.Remove(selectedId_);
    selectedId_=0;
}
void CollisionScene::ClearAll()
{
    world_.Clear();selectedId_=0;pendingPick_.reset();lastPick_.reset();
}

ColliderComponent* CollisionScene::Selected(){return world_.Find(selectedId_);}
const ColliderComponent* CollisionScene::Selected()const{return world_.Find(selectedId_);}

std::wstring CollisionScene::SceneStatusText()const
{
    return L"Collider: "+std::to_wstring(world_.Count())+
           L" / Contact: "+std::to_wstring(world_.Contacts().size())+
           L" / GPU 대기: "+std::to_wstring(gpuPicking_.PendingCount());
}

std::wstring CollisionScene::SelectionText()const
{
    const auto* c=Selected();
    if(!c)return L"선택: 없음 (3D Collider 클릭)";
    return L"선택: "+c->Name+L" / "+ShapeTypeName(GetShapeType(*c));
}

std::wstring CollisionScene::PickStatusText()const
{
    if(!gpuPickingEnabled_)return L"GPU Picking: OFF";
    if(pendingPick_)return L"GPU Picking: 요청 대기";
    if(!lastPick_)return L"GPU Picking: Scene 클릭 대기";
    if(!lastPick_->Hit)return L"GPU Picking: 배경";

    wchar_t buffer[160]={};
    swprintf_s(buffer,L"GPU Pick #%u  Depth %.3f  World(%.1f, %.1f, %.1f)",
               lastPick_->ObjectId,static_cast<double>(lastPick_->Depth),
               static_cast<double>(lastPick_->WorldPosition.x),
               static_cast<double>(lastPick_->WorldPosition.y),
               static_cast<double>(lastPick_->WorldPosition.z));
    return buffer;
}

std::wstring CollisionScene::ShapeParameterHint()const
{
    const auto* c=Selected();
    if(!c)return L"Shape 크기: Collider를 먼저 선택하세요.";
    switch(GetShapeType(*c))
    {
    case ShapeType::Sphere:return L"Sphere: Radius 사용 / Rotation·Half Extent 무시";
    case ShapeType::AABB:return L"AABB: Half Extent XYZ 사용 / Rotation 무시";
    case ShapeType::OBB:return L"OBB: Half Extent XYZ + Rotation XYZ 사용";
    case ShapeType::Capsule:return L"Capsule: Radius + Half Segment + Rotation 사용";
    }
    return L"";
}

bool CollisionScene::SelectedEnabled()const{const auto* c=Selected();return c&&c->Enabled;}
void CollisionScene::SetSelectedEnabled(bool v){if(auto* c=Selected())c->Enabled=v;}
bool CollisionScene::SelectedTrigger()const{const auto* c=Selected();return c&&c->IsTrigger;}
void CollisionScene::SetSelectedTrigger(bool v){if(auto* c=Selected())c->IsTrigger=v;}

float CollisionScene::GetPositionX()const{const auto* c=Selected();return c?c->Position.x:0.f;}
float CollisionScene::GetPositionY()const{const auto* c=Selected();return c?c->Position.y:0.f;}
float CollisionScene::GetPositionZ()const{const auto* c=Selected();return c?c->Position.z:0.f;}
void CollisionScene::SetPositionX(float v){if(auto* c=Selected())c->Position.x=v;}
void CollisionScene::SetPositionY(float v){if(auto* c=Selected())c->Position.y=v;}
void CollisionScene::SetPositionZ(float v){if(auto* c=Selected())c->Position.z=v;}

float CollisionScene::GetRotationXDegrees()const{const auto* c=Selected();return c?RadiansToDegrees(c->Rotation.x):0.f;}
float CollisionScene::GetRotationYDegrees()const{const auto* c=Selected();return c?RadiansToDegrees(c->Rotation.y):0.f;}
float CollisionScene::GetRotationZDegrees()const{const auto* c=Selected();return c?RadiansToDegrees(c->Rotation.z):0.f;}
void CollisionScene::SetRotationXDegrees(float v){if(auto* c=Selected())c->Rotation.x=DegreesToRadians(v);}
void CollisionScene::SetRotationYDegrees(float v){if(auto* c=Selected())c->Rotation.y=DegreesToRadians(v);}
void CollisionScene::SetRotationZDegrees(float v){if(auto* c=Selected())c->Rotation.z=DegreesToRadians(v);}

float CollisionScene::GetRadius()const
{
    const auto* c=Selected();if(!c)return 1.f;
    return std::visit([](const auto& shape)->float
    {
        using T=std::decay_t<decltype(shape)>;
        if constexpr(std::is_same_v<T,SphereShape>||std::is_same_v<T,CapsuleShape>)return shape.Radius;
        else return 1.f;
    },c->Shape);
}
void CollisionScene::SetRadius(float value)
{
    auto* c=Selected();if(!c)return;value=std::max(.1f,value);
    std::visit([&](auto& shape)
    {
        using T=std::decay_t<decltype(shape)>;
        if constexpr(std::is_same_v<T,SphereShape>||std::is_same_v<T,CapsuleShape>)shape.Radius=value;
    },c->Shape);
}

float CollisionScene::GetHalfSegment()const
{
    const auto* c=Selected();if(!c)return 1.f;
    if(const auto* shape=std::get_if<CapsuleShape>(&c->Shape))return shape->HalfSegment;
    return 1.f;
}
void CollisionScene::SetHalfSegment(float value)
{
    auto* c=Selected();if(!c)return;
    if(auto* shape=std::get_if<CapsuleShape>(&c->Shape))shape->HalfSegment=std::max(0.f,value);
}

namespace
{
    Vec3 CurrentExtents(const ColliderComponent* c)
    {
        if(!c)return {1.f,1.f,1.f};
        if(const auto* a=std::get_if<AABBShape>(&c->Shape))return a->HalfExtents;
        if(const auto* o=std::get_if<OBBShape>(&c->Shape))return o->HalfExtents;
        return {1.f,1.f,1.f};
    }
    void SetExtent(ColliderComponent* c,int axis,float value)
    {
        if(!c)return;value=std::max(.1f,value);
        auto apply=[&](Vec3& e){if(axis==0)e.x=value;else if(axis==1)e.y=value;else e.z=value;};
        if(auto* a=std::get_if<AABBShape>(&c->Shape))apply(a->HalfExtents);
        if(auto* o=std::get_if<OBBShape>(&c->Shape))apply(o->HalfExtents);
    }
}
float CollisionScene::GetHalfExtentX()const{return CurrentExtents(Selected()).x;}
float CollisionScene::GetHalfExtentY()const{return CurrentExtents(Selected()).y;}
float CollisionScene::GetHalfExtentZ()const{return CurrentExtents(Selected()).z;}
void CollisionScene::SetHalfExtentX(float v){SetExtent(Selected(),0,v);}
void CollisionScene::SetHalfExtentY(float v){SetExtent(Selected(),1,v);}
void CollisionScene::SetHalfExtentZ(float v){SetExtent(Selected(),2,v);}
