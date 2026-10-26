// ============================================================================
// CollisionDebugRenderer.h
// ----------------------------------------------------------------------------
// Collision Shape를 Line List로 시각화한다. 실제 Collision Detection과 완전히
// 분리되어 있어 이 Feature를 제거해도 Physics Core에는 영향이 없다.
// ============================================================================
#pragma once

#include "Graphics/ConstantBuffer.h"
#include "Graphics/Shader.h"
#include "Physics/Collision/Core/CollisionWorld.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cstddef>
#include <filesystem>
#include <string>

class CollisionDebugRenderer
{
public:
    bool Initialize(ID3D11Device* device,const std::filesystem::path& shaderDirectory);

    void Render(ID3D11DeviceContext* context,
                const DirectX::XMMATRIX& view,
                const DirectX::XMMATRIX& projection,
                const Collision::CollisionWorld& world,
                Collision::ColliderId selectedId,
                bool highlightCollisions,
                bool showContacts);

    const std::wstring& GetError()const{return error_;}

private:
    struct CBTransform
    {
        DirectX::XMFLOAT4X4 WorldViewProjection;
    };

    bool EnsureVertexCapacity(std::size_t vertexCount);

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState_;
    Shader shader_;
    ConstantBuffer<CBTransform> transformBuffer_;
    std::size_t vertexCapacity_=0;
    std::wstring error_;
};
