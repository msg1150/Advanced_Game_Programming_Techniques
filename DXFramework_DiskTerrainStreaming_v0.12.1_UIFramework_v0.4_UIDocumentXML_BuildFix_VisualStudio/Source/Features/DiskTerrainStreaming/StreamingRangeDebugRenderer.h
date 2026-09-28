// ============================================================================
// StreamingRangeDebugRenderer.h
// ----------------------------------------------------------------------------
// Terrain Streaming을 위한 카메라 방향 연동형 Mini Map.
//
// 이전 3D 원기둥 시각화는 지형 자체를 가렸으므로 완전히 제거한다.
// 디버그 표시만 교체하며 Streaming Policy / Worker / Chunk Renderer는 불변.
//
// - 녹색 원: 아직 로드되지 않은 Chunk를 새로 요청하는 Load Radius.
// - 주황 원: 이미 로드되었거나 Pending인 Chunk를 유지하는 Unload Radius.
// - 작은 격자: 실제 Chunk AABB 및 현재 Loaded / Pending / Unloaded 상태.
// - 흰 십자: Camera Position이 아닌 Camera Target(Streaming Focus).
//
// 별도 동적 VB/IB를 초기화 후 재사용하고 Frame마다 Map/Discard만 한다.
// ============================================================================
#pragma once

#include "Graphics/ConstantBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/Shader.h"

#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct StreamingRangeChunkInfo
{
    DirectX::BoundingBox WorldBounds;
    bool Loaded = false;
    bool Pending = false;
    bool Desired = false;
    bool Failed = false;
};

class StreamingRangeDebugRenderer
{
public:
    bool Initialize(
        ID3D11Device* device,
        const std::filesystem::path& basicShaderDirectory,
        float loadRadius,
        float unloadRadius,
        float minTerrainY,
        float maxTerrainY);

    // Orbit Camera의 Target은 Streaming 범위 중심과 동일한 World 좌표다.
    void SetFocus(
        const DirectX::XMFLOAT3& focusWorld,
        const DirectX::XMMATRIX& terrainWorld);

    // 상태 복사만 하며 Streaming 판정을 이곳에서 다시 구현하지 않는다.
    void SetChunks(const std::vector<StreamingRangeChunkInfo>& chunks);

    // Manager에서 사용하는 실제 Radius와 동기화한다.
    // Render에서는 절대 독립적으로 반경을 다시 계산하지 않는다.
    void SetRadii(float loadRadius,float unloadRadius);

    // 지도 표시 배율만 변경한다. 실제 Tile 판정/반경/카메라에는 영향 없음.
    // 50%=축소(넓은 지역), 100%=기본, 400%=확대(작은 지역).
    void SetZoomPercent(float percent);
    float GetZoomPercent() const;

    // View Matrix로 카메라 수평 Forward/Right를 얻어 미니맵과 나침반을 회전한다.
    // Projection은 이전과 마찬가지로 사용하지 않는다.
    void Render(
        ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        const DirectX::XMMATRIX& terrainWorld);

    const std::wstring& GetLastErrorMessage() const;

private:
    struct CBTransform
    {
        DirectX::XMFLOAT4X4 WorldViewProjection;
    };

    bool EnsureBufferCapacity(std::size_t vertices, std::size_t indices);

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Shader shader_;
    ConstantBuffer<CBTransform> transformBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> scissorState_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthOffState_;

    std::size_t vertexCapacity_ = 0u;
    std::size_t indexCapacity_ = 0u;

    DirectX::XMFLOAT3 focusWorld_ = {0.f, 0.f, 0.f};
    std::vector<StreamingRangeChunkInfo> chunks_;
    float loadRadius_ = 0.f;
    float unloadRadius_ = 0.f;
    float zoomPercent_ = 100.f; // 화면 안의 World Grid 배율. 패널 크기 불변.
    bool initialized_ = false;
    std::wstring lastError_;
};
