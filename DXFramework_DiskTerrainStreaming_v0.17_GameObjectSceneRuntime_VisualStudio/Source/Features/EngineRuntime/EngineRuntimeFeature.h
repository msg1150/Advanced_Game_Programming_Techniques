#pragma once
#include "Core/FeatureConfig.h"
#include "Engine/Core/Scene.h"
#include "Engine/Serialization/SceneSerializer.h"
#if DX_ENABLE_ENGINE_GRAPHICS
#include "Engine/Graphics/GraphicsResources.h"
#endif
#if DX_ENABLE_ENGINE_COLLISION
#include "Engine/Collision/CollisionBody.h"
#include "Features/CollisionEditor/CollisionDebugRenderer.h"
#endif
#include <Windows.h>
#include <DirectXMath.h>
#include <d3d11.h>
#include <filesystem>
class Input;class UIManager;

struct EngineRuntimeSettings
{
    std::filesystem::path InitialScene;
    float GroundClearance=.02f;
};
class EngineRuntimeFeature
{
public:
    using GroundProvider=std::function<bool(float,float,float&,Engine::Vec3&)>;
    bool Initialize(ID3D11Device* device,ID3D11DeviceContext* context,const std::filesystem::path& root,
                    EngineRuntimeSettings settings,GroundProvider ground);
    void Attach(UIManager& ui,HWND owner);
    void Update(const Input& input,float dt,bool keyboardAvailable);
    void Render(ID3D11DeviceContext* context,const DirectX::XMMATRIX& view,const DirectX::XMMATRIX& projection);
    bool CapturesMovementKeys()const{return playing_&&DX_ENABLE_ENGINE_GAMEPLAY;}
private:
    bool Load(const std::filesystem::path& path);
    void Save(HWND owner);void Open(HWND owner);void SelectNext();void DuplicateSelected();void GroundAll();
    void AddComponent(const std::string& type,const Engine::Properties& properties={});
    Engine::GameObject* Selected()const;
    Engine::Component* SelectedComponent()const;
    float GetTransform(unsigned field)const;void SetTransform(unsigned field,float value);
    std::wstring Status()const;
    EngineRuntimeSettings settings_;GroundProvider ground_;std::filesystem::path scenePath_;
    Engine::ObjectHandle selected_{};std::size_t componentIndex_=0;
    bool playing_=false,debug_=false,graphicsReady_=false,debugReady_=false;
    std::string status_;
#if DX_ENABLE_ENGINE_GRAPHICS
    Engine::GraphicsResources resources_;
#endif
#if DX_ENABLE_ENGINE_COLLISION
    Engine::CollisionBridge collision_;CollisionDebugRenderer debugRenderer_;
#endif
    Engine::ComponentRegistry registry_;
    Engine::SceneManager scenes_; // destroyed before adapters referenced by callbacks
};
