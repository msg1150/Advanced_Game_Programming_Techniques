// ============================================================================
// CollisionScene.h
// ----------------------------------------------------------------------------
// CollisionWorld + GPU Picking + Debug Renderer를 Runtime Scene에 연결하는 Feature.
// Application은 이 Facade만 알면 되며 Physics Core의 세부 Shape/알고리즘을 모른다.
// ============================================================================
#pragma once

#include "Features/CollisionEditor/CollisionDebugRenderer.h"
#include "Features/GPUPicking/GPUPickingSystem.h"
#include "Physics/Collision/Core/CollisionWorld.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <filesystem>
#include <optional>
#include <string>

class Camera;
class Input;
class UIManager;

class CollisionScene
{
public:
    bool Initialize(ID3D11Device* device,const std::filesystem::path& shaderRoot);
    void RegisterUI(UIManager& ui);

    // UI Callback이 새 Collider를 Camera Target 주변에 만들 수 있도록 Anchor 갱신.
    void SetSpawnAnchor(const DirectX::XMFLOAT3& target);

    // UI 입력이 아닌 Scene 클릭일 때만 GPU Picking 요청을 만든다.
    void UpdateInput(const Input& input,bool scenePointerAvailable);
    void UpdateCollisions();

    // Terrain이 Depth를 기록한 뒤 호출한다. 기존 Scene Depth를 그대로 사용해
    // 지형 뒤에 가려진 Collider가 Picking되는 문제를 줄인다.
    void ProcessGPUPicking(ID3D11DeviceContext* context,ID3D11DepthStencilView* sceneDepth,
                           const DirectX::XMMATRIX& view,const DirectX::XMMATRIX& projection,
                           UINT width,UINT height);

    void RenderDebug(ID3D11DeviceContext* context,
                     const DirectX::XMMATRIX& view,const DirectX::XMMATRIX& projection);

    const std::wstring& GetError()const{return error_;}

    // ---------------------------------------------------------------------
    // UI Binding API
    // ---------------------------------------------------------------------
    void AddSphere();
    void AddAABB();
    void AddOBB();
    void AddCapsule();
    void DeleteSelected();
    void ClearAll();
    void ClearSelection(){selectedId_=0;}

    Collision::ColliderComponent* Selected();
    const Collision::ColliderComponent* Selected()const;
    Collision::ColliderId SelectedId()const{return selectedId_;}

    std::wstring SceneStatusText()const;
    std::wstring SelectionText()const;
    std::wstring PickStatusText()const;
    std::wstring ShapeParameterHint()const;

    bool IsDebugVisible()const{return debugVisible_;}
    void SetDebugVisible(bool value){debugVisible_=value;}
    bool IsCollisionHighlightEnabled()const{return highlightCollisions_;}
    void SetCollisionHighlightEnabled(bool value){highlightCollisions_=value;}
    bool IsContactDebugEnabled()const{return showContacts_;}
    void SetContactDebugEnabled(bool value){showContacts_=value;}
    bool IsGPUPickingEnabled()const{return gpuPickingEnabled_;}
    void SetGPUPickingEnabled(bool value){gpuPickingEnabled_=value;if(!value)pendingPick_.reset();}

    bool SelectedEnabled()const;
    void SetSelectedEnabled(bool value);
    bool SelectedTrigger()const;
    void SetSelectedTrigger(bool value);

    float GetPositionX()const;float GetPositionY()const;float GetPositionZ()const;
    void SetPositionX(float v);void SetPositionY(float v);void SetPositionZ(float v);
    float GetRotationXDegrees()const;float GetRotationYDegrees()const;float GetRotationZDegrees()const;
    void SetRotationXDegrees(float v);void SetRotationYDegrees(float v);void SetRotationZDegrees(float v);

    float GetRadius()const;void SetRadius(float value);
    float GetHalfSegment()const;void SetHalfSegment(float value);
    float GetHalfExtentX()const;float GetHalfExtentY()const;float GetHalfExtentZ()const;
    void SetHalfExtentX(float value);void SetHalfExtentY(float value);void SetHalfExtentZ(float value);

private:
    struct PendingPick{int X=0,Y=0;};
    Collision::Vec3 NextSpawnPosition()const;

    Collision::CollisionWorld world_;
    CollisionDebugRenderer debugRenderer_;
    GPUPickingSystem gpuPicking_;

    Collision::ColliderId selectedId_=0;
    Collision::Vec3 spawnAnchor_{};
    std::optional<PendingPick> pendingPick_;
    std::optional<GPUPickResult> lastPick_;

    bool debugVisible_=true;
    bool highlightCollisions_=true;
    bool showContacts_=true;
    bool gpuPickingEnabled_=true;
    std::wstring error_;
};
