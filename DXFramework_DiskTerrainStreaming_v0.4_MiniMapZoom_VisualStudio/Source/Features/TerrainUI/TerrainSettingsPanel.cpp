// ============================================================================
// TerrainSettingsPanel.cpp : 기존 F1~F6 토글을 Checkbox로 완전히 이전.
// 실사용 Terrain 값을 Getter/Setter로 연결하여 표시와 실제 상태가 항상 일치.
// ============================================================================
#include "Features/TerrainUI/TerrainSettingsPanel.h"
#include <memory>
#include <string>
#include <cwchar>
#include <cstdio>
void TerrainSettingsPanel::Register(UIManager& ui,DiskTerrainManager& terrain,
                                    bool& debugVisible,bool& miniMapVisible)
{
    ui.Add(std::make_unique<UICheckBox>(L"Frustum Culling",
        [&terrain]{return terrain.IsCulling();},
        [&terrain](bool v){terrain.SetCulling(v);}));
    ui.Add(std::make_unique<UICheckBox>(L"QuadTree LOD",
        [&terrain]{return terrain.IsLOD();},
        [&terrain](bool v){terrain.SetLOD(v);}));
    ui.Add(std::make_unique<UICheckBox>(L"Debug Statistics",
        [&debugVisible]{return debugVisible;},
        [&debugVisible](bool v){debugVisible=v;}));
    ui.Add(std::make_unique<UICheckBox>(L"Tile Borders",
        [&terrain]{return terrain.IsBorders();},
        [&terrain](bool v){terrain.SetBorders(v);}));
    ui.Add(std::make_unique<UICheckBox>(L"Disk Streaming",
        [&terrain]{return terrain.IsStreaming();},
        [&terrain](bool v){terrain.SetStreaming(v);}));
    ui.Add(std::make_unique<UICheckBox>(L"Streaming MiniMap",
        [&miniMapVisible]{return miniMapVisible;},
        [&miniMapVisible](bool v){miniMapVisible=v;}));
    ui.Add(std::make_unique<UILabel>(L"Tile loading radius (independent of Zoom)"));
    ui.Add(std::make_unique<UISlider>(L"Load Radius",
        terrain.GetMinimumLoadRadius(),terrain.GetMaximumLoadRadius(),1.f,
        [&terrain]{return terrain.GetActiveLoadRadius();},
        [&terrain](float radius){terrain.SetLoadRadius(radius);}));
    ui.Add(std::make_unique<UILabel>([&terrain]
    {
        return L"Unload Radius: " +
            std::to_wstring(static_cast<int>(terrain.GetActiveUnloadRadius())) +
            L"  (margin +28)";
    }));
    // 패널 크기는 그대로, 격자/로딩 원만 함께 확대/축소한다.
    ui.Add(std::make_unique<UISlider>(L"MiniMap Zoom (%)",
        terrain.GetMinimumMiniMapZoomPercent(),
        terrain.GetMaximumMiniMapZoomPercent(),25.f,
        [&terrain]{return terrain.GetMiniMapZoomPercent();},
        [&terrain](float value){terrain.SetMiniMapZoomPercent(value);}));
}
void TerrainSettingsPanel::RenderStats(UIRenderer& renderer,
    const DiskTerrainManager& terrain,float distance,float width,bool visible)const
{
    if(!visible)return;
    const auto& stats=terrain.GetStats();
    constexpr UIColor foreground={.92f,.95f,.98f,1.f};
    constexpr UIColor muted={.65f,.75f,.83f,1.f};
    const float panelWidth=315.f;
    // 독립적인 좌상단 읽기 전용 패널. 우측의 설정 패널/미니맵과 분리.
    (void)width;
    const float x=12.f;
    const float y=12.f;
    renderer.FillRoundRect({x,y,panelWidth,309.f},{.065f,.09f,.13f,.95f},8.f);
    renderer.StrokeRect({x,y,panelWidth,309.f},{.29f,.38f,.46f,1.f});
    renderer.Text(L"Terrain Statistics",{x+13.f,y+7.f,290.f,31.f},
                  foreground,UIFont::Heading);
    float lineY=y+46.f;
    const auto line=[&](const std::wstring& label,const std::wstring& data)
    {
        renderer.Text(label,{x+14.f,lineY,145.f,25.f},muted,UIFont::Small);
        renderer.Text(data,{x+154.f,lineY,151.f,25.f},foreground,UIFont::Small);
        lineY+=24.f;
    };
    const auto num=[](auto value){return std::to_wstring(value);};
    wchar_t f[32]={};
    swprintf_s(f,L"%.1f",static_cast<double>(distance));
    line(L"Camera Zoom",f);
    swprintf_s(f,L"%.1f / %.1f",static_cast<double>(terrain.GetActiveLoadRadius()),
               static_cast<double>(terrain.GetActiveUnloadRadius()));
    line(L"Load / Unload",f);
    line(L"Loaded / Total",num(stats.LoadedTiles)+L" / "+num(stats.TotalTiles));
    line(L"Desired / Pending",num(stats.DesiredTiles)+L" / "+num(stats.PendingTiles));
    line(L"Visible / Culled",num(stats.VisibleTiles)+L" / "+num(stats.CulledTiles));
    line(L"Failed / Draw Calls",num(stats.FailedTiles)+L" / "+num(stats.DrawCalls));
    line(L"Triangles",num(stats.SurfaceTriangles));
    line(L"Disk Reads / KiB",num(stats.DiskReads)+L" / "+num(stats.DiskBytes/1024u));
    line(L"GPU buffers KiB",num(stats.ResidentGpuMeshBytes/1024u));
    if(!terrain.GetError().empty())
        renderer.Text(L"Tile error - see Visual Studio Output",{x+14.f,y+276.f,290.f,24.f},
                      {1.f,.48f,.42f,1.f},UIFont::Small);
}
