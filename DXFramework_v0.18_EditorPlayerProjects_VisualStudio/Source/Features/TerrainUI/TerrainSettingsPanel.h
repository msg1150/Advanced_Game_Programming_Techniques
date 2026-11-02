// ============================================================================
// TerrainSettingsPanel.h : Game Feature 설정을 재사용 가능한 UI Widget에 바인딩.
// UI Framework는 Terrain 클래스를 include하지 않고 독립적으로 유지한다.
// ============================================================================
#pragma once
#include "Features/DiskTerrainStreaming/DiskTerrainManager.h"
#include "UI/UIManager.h"
#include "UI/UIRenderer.h"
class TerrainSettingsPanel
{
public:
    void Register(UIManager& ui,DiskTerrainManager& terrain,
                  bool& debugVisible,bool& miniMapVisible);
    void RenderStats(UIRenderer& renderer,const DiskTerrainManager& terrain,
                     float cameraDistance,float clientWidth,bool debugVisible)const;
};
