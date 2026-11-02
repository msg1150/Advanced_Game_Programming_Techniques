// ============================================================================
// Application.h — 대형 Disk Terrain 하나만 초기화하고 표시하는 진입 지점.
// 이전 Showcase의 Feature 객체·GPU Mesh를 생성하지 않는다.
// ============================================================================
#pragma once
#include "Core/FeatureConfig.h"
#if DX_ENABLE_ENGINE_RUNTIME
#include "Features/EngineRuntime/EngineRuntimeFeature.h"
#endif
#if DX_ENABLE_SCENE_PERSISTENCE
#include "Features/ScenePersistence/ScenePersistenceFeature.h"
#endif
#if DX_ENABLE_TERRAIN_QUERY
#include "Terrain/Query/DiskTerrainQuerySource.h"
#endif
#if DX_ENABLE_TERRAIN_PLACEMENT
#include "Features/TerrainPlacement/TerrainPlacementFeature.h"
#endif
#include "Core/Timer.h"
#include "Features/DiskTerrainStreaming/DiskTerrainManager.h"
#include "Graphics/Camera.h"
#include "Graphics/CameraController.h"
#include "UI/UIRenderer.h"
#include "UI/UIManager.h"
#include "Features/TerrainUI/TerrainSettingsPanel.h"
#include "Features/UIDemo/UIDemoPanel.h"
#include "Features/CollisionEditor/CollisionScene.h"
#include "Tools/UIDesigner/Window/UIDesignerWindow.h"
#include "Graphics/Renderer.h"
#include "Input/Input.h"
#include "Platform/Window.h"
#include <Windows.h>
#include <string>
#include <memory>
class Application
{
public:
    bool Initialize(HINSTANCE instance);
    int Run();
    const std::wstring& GetLastErrorMessage() const;
private:
    void HandleWindowMessage(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
    void Update(float deltaTime);
    void Render();
    bool Fail(const wchar_t* stage,const std::wstring& message);
    Window window_;
    Input input_;
    Timer timer_;
    Renderer renderer_;
    Camera camera_;
    CameraController cameraController_;
    UIRenderer uiRenderer_;
    UIManager uiManager_;
    DiskTerrainManager terrain_;
    TerrainSettingsPanel terrainSettingsPanel_;
    UIDemoPanel uiDemoPanel_;
    CollisionScene collisionScene_;
#if DX_ENABLE_ENGINE_RUNTIME
    std::unique_ptr<EngineRuntimeFeature> engineRuntime_;
#endif
#if DX_ENABLE_SCENE_PERSISTENCE
    ScenePersistenceFeature scenePersistence_;
#endif
#if DX_ENABLE_TERRAIN_QUERY
    DiskTerrainQuerySource terrainQuerySource_;
    TerrainQuery terrainQuery_{terrainQuerySource_};
#endif
#if DX_ENABLE_TERRAIN_PLACEMENT
    std::unique_ptr<TerrainPlacementFeature> terrainPlacement_;
#endif
    UIDesignerWindow uiDesigner_;
    bool debugPanel_=false,miniMap_=true,initialized_=false;
    std::wstring lastError_;
};
