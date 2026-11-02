#include "Engine/Runtime/RuntimeSession.h"
#include "Engine/Components/ObjectLink.h"
#include "Core/Text/Utf8.h"
#if DX_ENABLE_ENGINE_GAMEPLAY
#include "Engine/Gameplay/Movement.h"
#endif
#include <fstream>

namespace Engine
{
bool RuntimeSession::Initialize(ID3D11Device* device,ID3D11DeviceContext* context,const std::filesystem::path& root,RuntimeSettings settings,GroundProvider ground)
{
    settings_=std::move(settings);ground_=std::move(ground);RegisterCoreComponents(registry_);
#if DX_ENABLE_ENGINE_GRAPHICS
    RegisterGraphicsComponents(registry_);if(!resources_.Initialize(device,context,root)){status_="Graphics resources initialization failed";return false;}
#endif
#if DX_ENABLE_WORLD_TERRAIN
    if(!settings_.TerrainDirectory.empty())
    {
        terrain_=std::make_unique<DiskTerrainManager>();DiskTerrainSettings ts;
        ts.LOD.LeafCellSize=8;ts.LOD.SplitDistanceFactor=2.5f;ts.LOD.SkirtDepth=.25f;
        TriplanarMaterialDesc material;const auto textures=root/L"Assets/Textures/Terrain";
        material.GrassTexturePath=textures/L"Grass.png";material.RockTexturePath=textures/L"Rock.png";material.SnowTexturePath=textures/L"Snow.png";
        material.ProjectionScale=.38f;material.BlendSharpness=4;material.GrassFadeStartHeight=.6f;material.GrassFadeEndHeight=3.5f;
        material.RockSlopeStart=.12f;material.RockSlopeEnd=.42f;material.SnowStartHeight=7;material.SnowFullHeight=12;
        if(!terrain_->Initialize(device,context,settings_.TerrainDirectory,root/L"Shaders/TriplanarTerrain",material,ts))
        {status_=TextUtf8::FromWide(terrain_->GetError());return false;}
#if DX_ENABLE_TERRAIN_QUERY
        std::wstring error;if(!querySource_.Initialize(settings_.TerrainDirectory,4u*1024u*1024u,error)){status_=TextUtf8::FromWide(error);return false;}
        ground_=[this](float x,float z,float& height,Vec3& normal)
        {const auto hit=query_.HeightAndNormal(x,z);if(hit.Status!=TerrainQueryStatus::Hit)return false;height=hit.Point.y;normal=hit.Normal;return true;};
#endif
    }
#endif
#if DX_ENABLE_ENGINE_COLLISION
    RegisterCollisionComponents(registry_);debugReady_=debugRenderer_.Initialize(device,root/L"Shaders/CollisionDebug");
#endif
#if DX_ENABLE_ENGINE_GAMEPLAY
    RegisterGameplayComponents(registry_,[this](GameObject& object,Vec3 displacement)
    {
#if DX_ENABLE_ENGINE_COLLISION
        collision_.Move(object,displacement,ground_,settings_.GroundClearance);
#else
        auto next=object.WorldMatrix().T+displacement;
        if(ground_){float height;Vec3 normal;if(!ground_(next.x,next.z,height,normal))return;next.y=height+settings_.GroundClearance;}
        object.SetWorldPosition(next);
#endif
    });
#endif
    (void)device;(void)context;(void)root;return Load(settings_.InitialScene);
}
bool RuntimeSession::Load(const std::filesystem::path& path)
{
    std::error_code ec;const auto bytes=std::filesystem::file_size(path,ec);
    if(ec||bytes>16u*1024u*1024u){status_="Scene unavailable or exceeds 16 MiB";return false;}
    std::ifstream in(path,std::ios::binary);std::unique_ptr<Scene> scene;std::vector<std::string> warnings;
    if(!SceneSerializer::Read(in,registry_,scene,status_,&warnings))return false;
#if DX_ENABLE_ENGINE_COLLISION
    scene->AfterFixedUpdate=[this](Scene& s,float){collision_.DetectAndDispatch(s);};
#endif
    scenes_.Request(std::move(scene));scenePath_=path;
    status_=warnings.empty()?"Scene loaded":"Scene loaded; missing components preserved: "+std::to_string(warnings.size());return true;
}
void RuntimeSession::Update(float dt,Vec3 input,bool simulate,const DirectX::XMFLOAT3& focus)
{
    // Apply pending scene transition before assigning input. A zero-delta update
    // retains v0.17 lifecycle behavior; separate Edit/Play worlds are a later stage.
    if(!scenes_.Current())scenes_.Update(0);
#if DX_ENABLE_ENGINE_GAMEPLAY
    if(auto* s=Current())for(auto* object:s->Objects())if(auto* movement=object->Get<Movement>())movement->InputDirection=simulate?input:Vec3{};
#endif
    scenes_.Update(simulate?dt:0);
    if(auto* s=Current())
    {
        s->Refresh();
#if DX_ENABLE_ENGINE_COLLISION
        collision_.Synchronize(*s);
#endif
    }
#if DX_ENABLE_WORLD_TERRAIN
    if(terrain_)terrain_->Update(focus,dt);
#endif
    (void)input;(void)focus;
}
void RuntimeSession::Render(ID3D11DeviceContext* context,const DirectX::XMMATRIX& view,const DirectX::XMMATRIX& projection,const DirectX::XMFLOAT3& position,bool debug)
{
#if DX_ENABLE_WORLD_TERRAIN
    if(terrain_)terrain_->Render(context,view,projection,position,false);
#endif
    auto* scene=Current();if(!scene)return;
#if DX_ENABLE_ENGINE_GRAPHICS
    resources_.Render(*scene,view,projection);
#endif
#if DX_ENABLE_ENGINE_COLLISION
    if(debug&&debugReady_)debugRenderer_.Render(context,view,projection,collision_.World(),0,true,true);
#endif
    (void)context;(void)view;(void)projection;(void)position;(void)debug;
}
void RuntimeSession::GroundAll()
{
    if(!Current()||!ground_){status_="No ground provider";return;}
    for(auto* object:Current()->Objects())
    {
#if DX_ENABLE_ENGINE_COLLISION
        if(object->Get<CollisionBody>())collision_.Move(*object,{},ground_,settings_.GroundClearance);
#else
        float h;Vec3 n;auto p=object->WorldMatrix().T;if(ground_(p.x,p.z,h,n)){p.y=h+settings_.GroundClearance;object->SetWorldPosition(p);}
#endif
    }
    status_="Ground placement requested";
}
void RuntimeSession::ClearResources()
{
#if DX_ENABLE_ENGINE_GRAPHICS
    resources_.Clear();
#endif
}
std::size_t RuntimeSession::ResourceCount()const
{
#if DX_ENABLE_ENGINE_GRAPHICS
    return resources_.ResourceCount();
#else
    return 0;
#endif
}
std::string RuntimeSession::ResourceError()const
{
#if DX_ENABLE_ENGINE_GRAPHICS
    return resources_.Error();
#else
    return {};
#endif
}
}
