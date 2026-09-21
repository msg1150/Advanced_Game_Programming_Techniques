// ============================================================================
// StreamingRangeDebugRenderer.cpp
// ----------------------------------------------------------------------------
// 화면 우하단에 작고 일정한 크기의 Top-Down Streaming Mini Map을 그린다.
// Camera Pitch나 Terrain 높이에 관계없이 판정 범위를 읽을 수 있게 한다.
// 실제 Load/Unload 결정 및 GPU Chunk 관리는 여기에서 전혀 수행하지 않는다.
// ============================================================================
#include "Features/DiskTerrainStreaming/StreamingRangeDebugRenderer.h"
#include "Features/DiskTerrainStreaming/MiniMapZoomPolicy.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

using namespace DirectX;

namespace
{
    constexpr std::uint32_t kCircleSegments = 96u;

    struct ScreenPoint
    {
        float X = 0.f;
        float Y = 0.f;
    };

    struct Geometry
    {
        std::vector<Vertex> Vertices;
        std::vector<std::uint32_t> TriangleIndices;
        std::vector<std::uint32_t> LineIndices;
    };
}

bool StreamingRangeDebugRenderer::Initialize(
    ID3D11Device* device,
    const std::filesystem::path& basicShaderDirectory,
    float loadRadius,
    float unloadRadius,
    float minTerrainY,
    float maxTerrainY)
{
    initialized_ = false;
    lastError_.clear();

    // 이전 호출 인터페이스를 유지하지만 Mini Map은 Y 높이에 의존하지 않는다.
    (void)minTerrainY;
    (void)maxTerrainY;

    if (!device || !(loadRadius > 0.f) || !(unloadRadius > loadRadius))
    {
        lastError_ = L"Streaming Mini Map: Device 또는 Radius 설정이 올바르지 않습니다.";
        return false;
    }

    device_ = device;
    loadRadius_ = loadRadius;
    unloadRadius_ = unloadRadius;
    zoomPercent_ = MiniMapZoomPolicy::DefaultPercent;

    if (!shader_.Initialize(
            device,
            basicShaderDirectory / L"BasicVS.hlsl",
            basicShaderDirectory / L"BasicPS.hlsl"))
    {
        lastError_ = L"Mini Map Shader 생성 실패: " + shader_.GetLastErrorMessage();
        return false;
    }

    if (!transformBuffer_.Initialize(device))
    {
        lastError_ = L"Mini Map Transform Constant Buffer 생성 실패";
        return false;
    }

    // 이 작은 화면 고정형 디버그 Pass에서만 깊이 검사를 끈다.
    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = FALSE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    if (FAILED(device->CreateDepthStencilState(
            &depthDesc, depthOffState_.GetAddressOf())))
    {
        lastError_ = L"Mini Map Depth State 생성 실패";
        return false;
    }

    // Mini Map이 지정된 화면 영역 밖으로 그려지지 않도록 Scissor 사용.
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.DepthClipEnable = TRUE;
    rasterDesc.ScissorEnable = TRUE;
    rasterDesc.AntialiasedLineEnable = TRUE;
    if (FAILED(device->CreateRasterizerState(
            &rasterDesc, scissorState_.GetAddressOf())))
    {
        lastError_ = L"Mini Map Rasterizer State 생성 실패";
        return false;
    }

    if (!EnsureBufferCapacity(512u, 1024u))
    {
        lastError_ = L"Mini Map 동적 Vertex / Index Buffer 생성 실패";
        return false;
    }

    initialized_ = true;
    return true;
}

void StreamingRangeDebugRenderer::SetFocus(
    const XMFLOAT3& focusWorld,
    const XMMATRIX& terrainWorld)
{
    (void)terrainWorld;
    focusWorld_ = focusWorld;
}

void StreamingRangeDebugRenderer::SetChunks(
    const std::vector<StreamingRangeChunkInfo>& chunks)
{
    chunks_ = chunks;
}

void StreamingRangeDebugRenderer::SetRadii(float loadRadius,float unloadRadius)
{
    // 디버그 표시가 잘못된 반경을 따로 보여주지 않도록 범위만 검증한다.
    if (!std::isfinite(loadRadius) || !std::isfinite(unloadRadius) ||
        loadRadius<=0.0f || unloadRadius<=loadRadius) return;
    loadRadius_=loadRadius;
    unloadRadius_=unloadRadius;
}

void StreamingRangeDebugRenderer::SetZoomPercent(float percent)
{
    // UI 외부에서 호출하더라도 상태를 유효 범위 안에 고정한다.
    zoomPercent_=MiniMapZoomPolicy::ClampPercent(percent);
}

float StreamingRangeDebugRenderer::GetZoomPercent() const
{
    return zoomPercent_;
}

bool StreamingRangeDebugRenderer::EnsureBufferCapacity(
    std::size_t vertices,
    std::size_t indices)
{
    if (!device_ || vertices > (std::numeric_limits<UINT>::max)() / sizeof(Vertex) ||
        indices > (std::numeric_limits<UINT>::max)() / sizeof(std::uint32_t))
    {
        return false;
    }

    // 카메라 이동 시 Buffer 재생성하지 않도록 충분한 Capacity를 보관한다.
    if (vertexCapacity_ < vertices)
    {
        const std::size_t capacity = std::max(vertices, vertexCapacity_ * 2u);
        if (capacity > (std::numeric_limits<UINT>::max)() / sizeof(Vertex)) return false;

        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = static_cast<UINT>(capacity * sizeof(Vertex));
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
        if (FAILED(device_->CreateBuffer(&desc, nullptr, buffer.GetAddressOf()))) return false;
        vertexBuffer_ = std::move(buffer);
        vertexCapacity_ = capacity;
    }

    if (indexCapacity_ < indices)
    {
        const std::size_t capacity = std::max(indices, indexCapacity_ * 2u);
        if (capacity > (std::numeric_limits<UINT>::max)() / sizeof(std::uint32_t)) return false;

        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = static_cast<UINT>(capacity * sizeof(std::uint32_t));
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
        if (FAILED(device_->CreateBuffer(&desc, nullptr, buffer.GetAddressOf()))) return false;
        indexBuffer_ = std::move(buffer);
        indexCapacity_ = capacity;
    }
    return true;
}

void StreamingRangeDebugRenderer::Render(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    const XMMATRIX& terrainWorld)
{
    (void)projection;
    (void)terrainWorld;
    if (!initialized_ || !context || chunks_.empty()) return;

    // ---------------------------------------------------------------------
    // Mini Map의 위쪽은 항상 카메라가 현재 바라보는 수평 방향이다.
    //
    // 이 프로젝트의 CameraController는 View 기준 Forward/Right로 WASD
    // 이동을 처리한다. 같은 View Matrix에서 축을 얻으므로 별도 Yaw
    // 복제나 입력 로직 수정 없이 미니맵의 이동 방향도 일치시킨다.
    //
    // Local View +Z = 카메라 Forward, Local View +X = 화면 Right.
    // Pitch 성분은 제거하고 XZ 평면에서 정규화한다.
    // ---------------------------------------------------------------------
    const XMMATRIX inverseView = XMMatrixInverse(nullptr, view);
    XMFLOAT3 cameraForward3D = {};
    XMFLOAT3 cameraRight3D = {};
    XMStoreFloat3(&cameraForward3D,
        XMVector3TransformNormal(XMVectorSet(0.f, 0.f, 1.f, 0.f), inverseView));
    XMStoreFloat3(&cameraRight3D,
        XMVector3TransformNormal(XMVectorSet(1.f, 0.f, 0.f, 0.f), inverseView));

    const float forwardLength = std::sqrt(
        cameraForward3D.x * cameraForward3D.x +
        cameraForward3D.z * cameraForward3D.z);
    const float rightLength = std::sqrt(
        cameraRight3D.x * cameraRight3D.x +
        cameraRight3D.z * cameraRight3D.z);
    if (forwardLength < 0.0001f || rightLength < 0.0001f) return;

    const float forwardX = cameraForward3D.x / forwardLength;
    const float forwardZ = cameraForward3D.z / forwardLength;
    const float rightX = cameraRight3D.x / rightLength;
    const float rightZ = cameraRight3D.z / rightLength;

    // Mini Map 전용 좌표:
    // u > 0 = 화면 오른쪽 / v > 0 = 화면 아래쪽.
    const auto toMap = [&](float x, float z) -> ScreenPoint
    {
        const float dx = x - focusWorld_.x;
        const float dz = z - focusWorld_.z;
        return {dx * rightX + dz * rightZ,
               -(dx * forwardX + dz * forwardZ)};
    };

    UINT viewportCount = 1u;
    D3D11_VIEWPORT viewport = {};
    context->RSGetViewports(&viewportCount, &viewport);
    if (viewportCount == 0u || viewport.Width < 180.f || viewport.Height < 180.f)
    {
        return;
    }

    // 화면 우하단 260px 이하의 고정형 영역만 차지한다.
    const float panelSide = std::min(
        268.f,
        std::min(viewport.Width, viewport.Height) - 24.f);
    const float panelLeft = viewport.TopLeftX + viewport.Width - panelSide - 12.f;
    const float panelTop = viewport.TopLeftY + viewport.Height - panelSide - 12.f;
    const float panelRight = panelLeft + panelSide;
    const float panelBottom = panelTop + panelSide;
    const float mapSide = panelSide - 48.f;
    const float mapLeft = panelLeft + (panelSide - mapSide) * 0.5f;
    const float mapTop = panelTop + 10.f;

    // ---------------------------------------------------------------------
    // Load/Unload 원의 실제 World Radius는 고정이다.
    // UI의 MiniMap Zoom은 그려지는 격자/원 전체의 화면 배율만 조절한다.
    // 미니맵 바깥 사각형(패널 크기), Camera Zoom, Tile 로딩에는 영향 없음.
    // 100%는 이전 버전과 정확히 같은 배율이다.
    // ---------------------------------------------------------------------
    const float viewRadius=MiniMapZoomPolicy::ViewRadius(unloadRadius_,zoomPercent_);
    const float scale=mapSide/(2.f*viewRadius);
    const auto toScreen = [&](float x, float z) -> ScreenPoint
    {
        const ScreenPoint p = toMap(x, z);
        return { mapLeft + 0.5f * mapSide + p.X * scale,
                 mapTop + 0.5f * mapSide + p.Y * scale };
    };

    Geometry geometry;
    geometry.Vertices.reserve(chunks_.size() * 8u + kCircleSegments * 4u + 64u);
    geometry.TriangleIndices.reserve(chunks_.size() * 6u + 128u);
    geometry.LineIndices.reserve(chunks_.size() * 8u + kCircleSegments * 4u + 64u);

    // Screen Pixel -> NDC. Identity WVP로 Basic Shader를 재사용한다.
    const auto vertexAt = [&](ScreenPoint p, const XMFLOAT4& color) -> std::uint32_t
    {
        const float x = 2.f * (p.X - viewport.TopLeftX) / viewport.Width - 1.f;
        const float y = 1.f - 2.f * (p.Y - viewport.TopLeftY) / viewport.Height;
        const auto index = static_cast<std::uint32_t>(geometry.Vertices.size());
        geometry.Vertices.push_back({{x,y,0.f},{0.f,1.f,0.f},{0.f,0.f},color});
        return index;
    };

    const auto fillRect = [&](float l,float t,float r,float b,const XMFLOAT4& color)
    {
        const auto a=vertexAt({l,t},color);
        const auto c=vertexAt({r,t},color);
        const auto d=vertexAt({r,b},color);
        const auto e=vertexAt({l,b},color);
        auto& ix=geometry.TriangleIndices;
        ix.insert(ix.end(),{a,c,d,a,d,e});
    };
    // 회전된 Chunk는 더 이상 화면 축에 평행한 사각형이 아니다.
    // 네 꼭짓점을 직접 삼각형 두 개로 그려야 Grid와 실제 월드 회전이 일치한다.
    const auto fillQuad = [&](ScreenPoint a,ScreenPoint b,
                              ScreenPoint c,ScreenPoint d,const XMFLOAT4& color)
    {
        const auto i0=vertexAt(a,color);
        const auto i1=vertexAt(b,color);
        const auto i2=vertexAt(c,color);
        const auto i3=vertexAt(d,color);
        geometry.TriangleIndices.insert(geometry.TriangleIndices.end(),
            {i0,i1,i2,i0,i2,i3});
    };
    const auto line = [&](ScreenPoint a,ScreenPoint b,const XMFLOAT4& color)
    {
        const auto i=vertexAt(a,color);
        const auto j=vertexAt(b,color);
        geometry.LineIndices.push_back(i);
        geometry.LineIndices.push_back(j);
    };
    const auto outline = [&](float l,float t,float r,float b,const XMFLOAT4& color)
    {
        line({l,t},{r,t},color);
        line({r,t},{r,b},color);
        line({r,b},{l,b},color);
        line({l,b},{l,t},color);
    };
    const auto circle = [&](float radius,const XMFLOAT4& color)
    {
        for(std::uint32_t i=0u;i<kCircleSegments;++i)
        {
            const float angleA=XM_2PI*static_cast<float>(i)/kCircleSegments;
            const float angleB=XM_2PI*static_cast<float>((i+1u)%kCircleSegments)/kCircleSegments;
            line(toScreen(focusWorld_.x+std::cos(angleA)*radius,
                          focusWorld_.z+std::sin(angleA)*radius),
                 toScreen(focusWorld_.x+std::cos(angleB)*radius,
                          focusWorld_.z+std::sin(angleB)*radius),color);
        }
    };

    // 불투명한 작은 배경만 그린다. 기존 F3 텍스트 패널과 독립적이다.
    const XMFLOAT4 panel={0.025f,0.033f,0.045f,1.f};
    const XMFLOAT4 mapBackground={0.07f,0.085f,0.10f,1.f};
    const XMFLOAT4 grid={0.69f,0.76f,0.80f,1.f};
    const XMFLOAT4 loaded={0.12f,0.58f,0.43f,1.f};
    const XMFLOAT4 pending={0.91f,0.69f,0.19f,1.f};
    const XMFLOAT4 desired={0.26f,0.48f,0.76f,1.f};
    const XMFLOAT4 unloaded={0.24f,0.27f,0.31f,1.f};
    const XMFLOAT4 failed={0.86f,0.25f,0.28f,1.f};
    const XMFLOAT4 loadLine={0.14f,1.f,0.25f,1.f};
    const XMFLOAT4 unloadLine={1.f,0.57f,0.10f,1.f};
    const XMFLOAT4 white={1.f,1.f,1.f,1.f};

    fillRect(panelLeft,panelTop,panelRight,panelBottom,panel);
    fillRect(mapLeft,mapTop,mapLeft+mapSide,mapTop+mapSide,mapBackground);
    const std::uint32_t backgroundTriangleIndices=
        static_cast<std::uint32_t>(geometry.TriangleIndices.size());

    for(const auto& chunk: chunks_)
    {
        // 화면 중심에서 멀리 떨어진 Tile의 UI Geometry 생성을 건너뛴다.
        const float dx = chunk.WorldBounds.Center.x - focusWorld_.x;
        const float dz = chunk.WorldBounds.Center.z - focusWorld_.z;
        const float extent = chunk.WorldBounds.Extents.x + chunk.WorldBounds.Extents.z;
        if(std::abs(dx) > viewRadius * 1.5f + extent ||
           std::abs(dz) > viewRadius * 1.5f + extent) continue;

        const float x0=chunk.WorldBounds.Center.x-chunk.WorldBounds.Extents.x;
        const float x1=chunk.WorldBounds.Center.x+chunk.WorldBounds.Extents.x;
        const float z0=chunk.WorldBounds.Center.z-chunk.WorldBounds.Extents.z;
        const float z1=chunk.WorldBounds.Center.z+chunk.WorldBounds.Extents.z;
        const ScreenPoint a=toScreen(x0,z0);
        const ScreenPoint b=toScreen(x1,z0);
        const ScreenPoint c=toScreen(x1,z1);
        const ScreenPoint d=toScreen(x0,z1);
        const XMFLOAT4& color = chunk.Failed ? failed : chunk.Loaded ? loaded :
                                chunk.Pending ? pending : chunk.Desired ? desired : unloaded;
        fillQuad(a,b,c,d,color);
        line(a,b,grid);
        line(b,c,grid);
        line(c,d,grid);
        line(d,a,grid);
    }
    const std::uint32_t mapTriangleEnd=
        static_cast<std::uint32_t>(geometry.TriangleIndices.size());

    circle(unloadRadius_,unloadLine);
    circle(loadRadius_,loadLine);
    const ScreenPoint focus=toScreen(focusWorld_.x,focusWorld_.z);
    line({focus.X-6.f,focus.Y},{focus.X+6.f,focus.Y},white);
    line({focus.X,focus.Y-6.f},{focus.X,focus.Y+6.f},white);
    const std::uint32_t mapLineIndices=
        static_cast<std::uint32_t>(geometry.LineIndices.size());
    outline(mapLeft,mapTop,mapLeft+mapSide,mapTop+mapSide,grid);

    // ---------------------------------------------------------------------
    // 우상단 작은 나침반: N/E/S/W(북/동/남/서)를 문자 형태의 선으로 직접
    // 그린다. 새 폰트, D2D RenderTarget, 공용 Renderer 변경이 필요 없다.
    //
    // 프로젝트 좌표 약속: +Z=N, +X=E, -Z=S, -X=W.
    // 방향 글자는 월드 축을 따라 지도와 함께 회전하지만,
    // 카메라가 바라보는 방향 화살표는 항상 화면 위쪽을 가리킨다.
    // ---------------------------------------------------------------------
    const ScreenPoint compass={mapLeft+mapSide-30.f,mapTop+30.f};
    fillRect(compass.X-29.f,compass.Y-29.f,
             compass.X+29.f,compass.Y+29.f,panel);
    const auto compassCircle = [&]()
    {
        constexpr std::uint32_t kCompassSegments=32u;
        for(std::uint32_t i=0u;i<kCompassSegments;++i)
        {
            const float a=XM_2PI*static_cast<float>(i)/kCompassSegments;
            const float b=XM_2PI*static_cast<float>(i+1u)/kCompassSegments;
            line({compass.X+12.f*std::cos(a),compass.Y+12.f*std::sin(a)},
                 {compass.X+12.f*std::cos(b),compass.Y+12.f*std::sin(b)},grid);
        }
    };
    compassCircle();
    line({compass.X,compass.Y+7.f},{compass.X,compass.Y-7.f},white);
    line({compass.X,compass.Y-7.f},{compass.X-3.f,compass.Y-3.f},white);
    line({compass.X,compass.Y-7.f},{compass.X+3.f,compass.Y-3.f},white);

    const XMFLOAT4 northColor={1.f,0.91f,0.35f,1.f};
    const auto drawLetter = [&](char letter, ScreenPoint center, const XMFLOAT4& color)
    {
        // 7×9 픽셀 벡터 글자. 회전 중에도 문자는 똑바로 읽을 수 있다.
        const float l=center.X-3.5f;
        const float r=center.X+3.5f;
        const float t=center.Y-4.5f;
        const float b=center.Y+4.5f;
        const float m=center.Y;
        switch(letter)
        {
        case 'N':
            line({l,b},{l,t},color);
            line({l,t},{r,b},color);
            line({r,b},{r,t},color);
            break;
        case 'E':
            line({r,t},{l,t},color);
            line({l,t},{l,b},color);
            line({l,m},{r,m},color);
            line({l,b},{r,b},color);
            break;
        case 'S':
            line({r,t},{l,t},color);
            line({l,t},{l,m},color);
            line({l,m},{r,m},color);
            line({r,m},{r,b},color);
            line({r,b},{l,b},color);
            break;
        case 'W':
            line({l,t},{l,b},color);
            line({l,b},{center.X,center.Y+1.5f},color);
            line({center.X,center.Y+1.5f},{r,b},color);
            line({r,b},{r,t},color);
            break;
        default: break;
        }
    };
    const auto compassLabel = [&](char label,float worldX,float worldZ,
                                  const XMFLOAT4& color)
    {
        constexpr float kLetterDistance=22.f;
        const float u=worldX*rightX+worldZ*rightZ;
        const float v=-(worldX*forwardX+worldZ*forwardZ);
        drawLetter(label,{compass.X+u*kLetterDistance,
                          compass.Y+v*kLetterDistance},color);
    };
    compassLabel('N',0.f,+1.f,northColor);
    compassLabel('E',+1.f,0.f,white);
    compassLabel('S',0.f,-1.f,white);
    compassLabel('W',-1.f,0.f,white);

    // 짧은 색상 키. 상세한 텍스트 해설은 기존 F3 패널에서 제공한다.
    const float legendY=mapTop+mapSide+9.f;
    const XMFLOAT4 legendColors[5]={loadLine,unloadLine,loaded,pending,unloaded};
    for(std::size_t i=0;i<5u;++i)
    {
        const float x=panelLeft+13.f+static_cast<float>(i)*((panelSide-26.f)/5.f);
        fillRect(x,legendY,x+((panelSide-26.f)/5.f)-6.f,legendY+7.f,legendColors[i]);
    }

    // Triangle / Line은 한 Index Buffer에 연속 배치하여 DrawIndexed 2회만 호출.
    const std::uint32_t triangleCount=static_cast<std::uint32_t>(geometry.TriangleIndices.size());
    std::vector<std::uint32_t> indices=std::move(geometry.TriangleIndices);
    indices.insert(indices.end(),geometry.LineIndices.begin(),geometry.LineIndices.end());
    if(indices.empty() || geometry.Vertices.empty() ||
       !EnsureBufferCapacity(geometry.Vertices.size(),indices.size())) return;

    D3D11_MAPPED_SUBRESOURCE mapped={};
    if(FAILED(context->Map(vertexBuffer_.Get(),0u,D3D11_MAP_WRITE_DISCARD,0u,&mapped))) return;
    std::memcpy(mapped.pData,geometry.Vertices.data(),geometry.Vertices.size()*sizeof(Vertex));
    context->Unmap(vertexBuffer_.Get(),0u);
    if(FAILED(context->Map(indexBuffer_.Get(),0u,D3D11_MAP_WRITE_DISCARD,0u,&mapped))) return;
    std::memcpy(mapped.pData,indices.data(),indices.size()*sizeof(std::uint32_t));
    context->Unmap(indexBuffer_.Get(),0u);

    // 기존 Terrain Rendering State를 보관한다.
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> previousRS;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> previousDepth;
    Microsoft::WRL::ComPtr<ID3D11BlendState> previousBlend;
    UINT previousStencilRef=0u;
    UINT previousSampleMask=0u;
    FLOAT previousBlendFactor[4]={};
    D3D11_PRIMITIVE_TOPOLOGY previousTopology=D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    context->RSGetState(previousRS.GetAddressOf());
    context->OMGetDepthStencilState(previousDepth.GetAddressOf(),&previousStencilRef);
    context->OMGetBlendState(previousBlend.GetAddressOf(),previousBlendFactor,&previousSampleMask);
    context->IAGetPrimitiveTopology(&previousTopology);

    UINT originalScissorCount=D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    D3D11_RECT previousScissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]={};
    context->RSGetScissorRects(&originalScissorCount,previousScissors);

    context->RSSetState(scissorState_.Get());
    const D3D11_RECT panelScissor={
        static_cast<LONG>(std::floor(panelLeft)),
        static_cast<LONG>(std::floor(panelTop)),
        static_cast<LONG>(std::ceil(panelRight)),
        static_cast<LONG>(std::ceil(panelBottom))};
    const D3D11_RECT mapScissor={
        static_cast<LONG>(std::floor(mapLeft)),
        static_cast<LONG>(std::floor(mapTop)),
        static_cast<LONG>(std::ceil(mapLeft+mapSide)),
        static_cast<LONG>(std::ceil(mapTop+mapSide))};
    context->RSSetScissorRects(1u,&panelScissor);
    context->OMSetDepthStencilState(depthOffState_.Get(),0u);
    const FLOAT blendFactor[4]={0.f,0.f,0.f,0.f};
    context->OMSetBlendState(nullptr,blendFactor,D3D11_DEFAULT_SAMPLE_MASK);

    shader_.Bind(context);
    CBTransform transform={};
    XMStoreFloat4x4(&transform.WorldViewProjection,XMMatrixIdentity());
    transformBuffer_.Update(context,transform);
    transformBuffer_.BindVS(context,0u);

    ID3D11Buffer* vb=vertexBuffer_.Get();
    const UINT stride=sizeof(Vertex);
    const UINT offset=0u;
    context->IASetVertexBuffers(0u,1u,&vb,&stride,&offset);
    context->IASetIndexBuffer(indexBuffer_.Get(),DXGI_FORMAT_R32_UINT,0u);
    // UI 배율을 크게 해도 격자/원 선이 미니맵 내부 바깥으로 튀지 않도록
    // 월드 지도 도형만 mapScissor, 패널/나침반은 panelScissor를 사용한다.
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    if(backgroundTriangleIndices>0u)
        context->DrawIndexed(backgroundTriangleIndices,0u,0);
    context->RSSetScissorRects(1u,&mapScissor);
    if(mapTriangleEnd>backgroundTriangleIndices)
        context->DrawIndexed(mapTriangleEnd-backgroundTriangleIndices,
                             backgroundTriangleIndices,0);
    context->RSSetScissorRects(1u,&panelScissor);
    if(triangleCount>mapTriangleEnd)
        context->DrawIndexed(triangleCount-mapTriangleEnd,mapTriangleEnd,0);

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    context->RSSetScissorRects(1u,&mapScissor);
    if(mapLineIndices>0u)
        context->DrawIndexed(mapLineIndices,triangleCount,0);
    context->RSSetScissorRects(1u,&panelScissor);
    const auto remainingLines=static_cast<UINT>(indices.size()-triangleCount)-mapLineIndices;
    if(remainingLines>0u)
        context->DrawIndexed(remainingLines,triangleCount+mapLineIndices,0);

    // 후속 DebugTextRenderer 및 다음 Frame에 상태가 누출되지 않도록 복구.
    context->IASetPrimitiveTopology(previousTopology);
    context->OMSetBlendState(previousBlend.Get(),previousBlendFactor,previousSampleMask);
    context->OMSetDepthStencilState(previousDepth.Get(),previousStencilRef);
    context->RSSetState(previousRS.Get());
    if(originalScissorCount>0u)
    {
        context->RSSetScissorRects(originalScissorCount,previousScissors);
    }
}

const std::wstring& StreamingRangeDebugRenderer::GetLastErrorMessage() const
{
    return lastError_;
}
