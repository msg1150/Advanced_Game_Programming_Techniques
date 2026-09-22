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

    ui.StartSecondColumn();
    ui.Add(std::make_unique<UILabel>(L"Predictive Prefetch / CPU Cache"));
    ui.Add(std::make_unique<UICheckBox>(L"Predictive Prefetch",
        [&terrain]{return terrain.IsPrefetch();},
        [&terrain](bool value){terrain.SetPrefetch(value);}));
    ui.Add(std::make_unique<UICheckBox>(L"CPU Tile Sample Cache",
        [&terrain]{return terrain.IsCache();},
        [&terrain](bool value){terrain.SetCache(value);}));
    ui.Add(std::make_unique<UILabel>(L"Ahead of camera movement (world units)"));
    ui.Add(std::make_unique<UISlider>(L"Prefetch Lead",
        0.f,96.f,1.f,
        [&terrain]{return terrain.GetPrefetchLead();},
        [&terrain](float value){terrain.SetPrefetchLead(value);}));
    ui.Add(std::make_unique<UILabel>(L"CPU height samples LRU budget"));
    ui.Add(std::make_unique<UISlider>(L"Cache Budget (MiB)",
        1.f,16.f,1.f,
        [&terrain]{return terrain.GetCacheBudgetMiB();},
        [&terrain](float value){terrain.SetCacheBudgetMiB(value);}));
    ui.Add(std::make_unique<UILabel>(L"Wheel: camera only / Load: slider only"));
}
void TerrainSettingsPanel::RenderStats(UIRenderer& renderer,
    const DiskTerrainManager& terrain,float distance,float width,bool visible)const
{
    if(!visible)return;
    const auto& stats=terrain.GetStats();
    constexpr UIColor foreground={.92f,.95f,.98f,1.f};
    constexpr UIColor muted={.65f,.75f,.83f,1.f};
    const float panelWidth=340.f;
    // 독립적인 좌상단 읽기 전용 패널. 우측의 설정 패널/미니맵과 분리.
    (void)width;
    const float x=12.f;
    const float y=12.f;
    // 기존 13줄 → 19줄. 모든 수치를 보여도 패널 안에서 잘리지 않도록 확장.
    renderer.FillRoundRect({x,y,panelWidth,570.f},{.065f,.09f,.13f,.95f},8.f);
    renderer.StrokeRect({x,y,panelWidth,570.f},{.29f,.38f,.46f,1.f});
    renderer.Text(L"Terrain Statistics",{x+13.f,y+7.f,290.f,31.f},
                  foreground,UIFont::Heading);
    float lineY=y+46.f;
    const auto line=[&](const std::wstring& label,const std::wstring& data)
    {
        renderer.Text(label,{x+14.f,lineY,162.f,25.f},muted,UIFont::Small);
        renderer.Text(data,{x+178.f,lineY,153.f,25.f},foreground,UIFont::Small);
        lineY+=24.f;
    };
    const auto num=[](auto value){return std::to_wstring(value);};
    wchar_t f[64]={};
    const auto msPair=[](const StreamingTiming& timing)->std::wstring
    {
        if(timing.Count==0u)return L"- / -"; // 아직 샘플이 없으면 0 ms로 오해하지 않도록.
        wchar_t buffer[64]={};
        swprintf_s(buffer,L"%.2f / %.2f",timing.LastMilliseconds(),
                   timing.AverageMilliseconds());
        return buffer;
    };
    const auto msTotal=[](const StreamingTiming& timing)->std::wstring
    {
        if(timing.Count==0u)return L"-";
        wchar_t buffer[64]={};
        swprintf_s(buffer,L"%.2f",timing.TotalMilliseconds());
        return buffer;
    };
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
    line(L"Prefetch disk reads",num(stats.PrefetchReads));
    line(L"Cache tiles / KiB",num(stats.CachedTiles)+L" / "+
                                  num(stats.CachedSampleBytes/1024u));
    line(L"Cache hits / misses",num(stats.CacheHits)+L" / "+num(stats.CacheMisses));
    line(L"Cache evictions",num(stats.CacheEvictions));
    line(L"Prefetch queue / fail",num(stats.PrefetchQueued)+L" / "+
                                      num(stats.PrefetchFailures));
    // L/A: 마지막 처리 시간 / 성공한 작업의 누적 평균 (ms).
    line(L"Disk read ms L/A",msPair(stats.DiskReadTime));
    line(L"Disk read total ms",msTotal(stats.DiskReadTime));
    line(L"Tile load ms L/A",msPair(stats.TileLoadTime));
    line(L"Tile load total ms",msTotal(stats.TileLoadTime));
    line(L"Tile loads completed",num(stats.TileLoadTime.Count));
    if(!terrain.GetError().empty())
        renderer.Text(L"Tile error - see Visual Studio Output",{x+14.f,y+532.f,315.f,24.f},
                      {1.f,.48f,.42f,1.f},UIFont::Small);
}
