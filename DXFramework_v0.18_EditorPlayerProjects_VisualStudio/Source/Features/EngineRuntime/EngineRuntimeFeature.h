#pragma once
#include "Engine/Runtime/RuntimeSession.h"
#include <Windows.h>
class Input;class UIManager;
using EngineRuntimeSettings=Engine::RuntimeSettings;
class EngineRuntimeFeature
{
public:
    using GroundProvider=Engine::RuntimeSession::GroundProvider;
    bool Initialize(ID3D11Device*,ID3D11DeviceContext*,const std::filesystem::path& root,EngineRuntimeSettings,GroundProvider={});
    void Attach(UIManager&,HWND);
    void Update(const Input&,float dt,bool keyboardAvailable,const DirectX::XMFLOAT3& focus={});
    void Render(ID3D11DeviceContext*,const DirectX::XMMATRIX&,const DirectX::XMMATRIX&,const DirectX::XMFLOAT3& cameraPosition={});
    bool CapturesMovementKeys()const{return playing_&&DX_ENABLE_ENGINE_GAMEPLAY;}
    Engine::Scene* Current()const{return session_?session_->Current():nullptr;}
    std::filesystem::path ScenePath()const{return session_?session_->ScenePath():std::filesystem::path{};}
    const std::string& Message()const{return status_;}
private:
    bool Load(const std::filesystem::path&);
    void Save(HWND);void Open(HWND);void SelectNext();void DuplicateSelected();void GroundAll();
    void AddComponent(const std::string&,const Engine::Properties& properties={});
    Engine::GameObject* Selected()const;Engine::Component* SelectedComponent()const;
    float GetTransform(unsigned)const;void SetTransform(unsigned,float);std::wstring Status()const;
    std::filesystem::path root_,scenePath_;Engine::ObjectHandle selected_{};std::size_t componentIndex_=0;
    bool playing_=false,debug_=false;std::string status_;
    std::unique_ptr<Engine::RuntimeSession> session_;
};
