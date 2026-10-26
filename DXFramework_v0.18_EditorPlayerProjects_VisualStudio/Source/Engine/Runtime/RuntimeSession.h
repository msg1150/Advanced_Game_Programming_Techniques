#pragma once
#include "Core/FeatureConfig.h"
#include "Engine/Core/Scene.h"
#include "Engine/Serialization/SceneSerializer.h"
#if DX_ENABLE_ENGINE_GRAPHICS
#include "Engine/Graphics/GraphicsResources.h"
#endif
#if DX_ENABLE_ENGINE_COLLISION
#include "Engine/Collision/CollisionBody.h"
#include "Graphics/Debug/CollisionDebugRenderer.h"
#endif
#if DX_ENABLE_WORLD_TERRAIN
#include "Features/DiskTerrainStreaming/DiskTerrainManager.h"
#if DX_ENABLE_TERRAIN_QUERY
#include "Terrain/Query/DiskTerrainQuerySource.h"
#endif
#endif
#include <d3d11.h>
#include <DirectXMath.h>

namespace Engine
{
struct RuntimeSettings
{
    std::filesystem::path InitialScene,TerrainDirectory;
    float GroundClearance=.02f;
};
// Per-world runtime, shared by Editor and Player. No editor UI/file dialog/process APIs.
class RuntimeSession
{
public:
    using GroundProvider=std::function<bool(float,float,float&,Vec3&)>;
    bool Initialize(ID3D11Device*,ID3D11DeviceContext*,const std::filesystem::path& assetRoot,RuntimeSettings,GroundProvider={});
    bool Load(const std::filesystem::path& sceneFile);
    void Update(float dt,Vec3 input,bool simulate,const DirectX::XMFLOAT3& focus={});
    void Render(ID3D11DeviceContext*,const DirectX::XMMATRIX&,const DirectX::XMMATRIX&,const DirectX::XMFLOAT3& cameraPosition={},bool debug=false);
    void GroundAll();
    Scene* Current()const{return scenes_.Current();}
    ComponentRegistry& Registry(){return registry_;}
    const std::string& Status()const{return status_;}
    const std::filesystem::path& ScenePath()const{return scenePath_;}
    void ClearResources();std::size_t ResourceCount()const;
    std::string ResourceError()const;
private:
    RuntimeSettings settings_;GroundProvider ground_;std::filesystem::path scenePath_;std::string status_;
#if DX_ENABLE_WORLD_TERRAIN
    std::unique_ptr<DiskTerrainManager> terrain_;
#if DX_ENABLE_TERRAIN_QUERY
    DiskTerrainQuerySource querySource_;TerrainQuery query_{querySource_};
#endif
#endif
#if DX_ENABLE_ENGINE_GRAPHICS
    GraphicsResources resources_;
#endif
#if DX_ENABLE_ENGINE_COLLISION
    CollisionBridge collision_;CollisionDebugRenderer debugRenderer_;bool debugReady_=false;
#endif
    ComponentRegistry registry_;SceneManager scenes_; // callbacks die before their services
};
}
