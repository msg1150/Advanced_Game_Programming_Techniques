// ============================================================================
// CollisionDebugRenderer.cpp
// ============================================================================
#include "Features/CollisionEditor/CollisionDebugRenderer.h"
#include "Features/CollisionEditor/CollisionGeometry.h"
#include "Graphics/Mesh.h"

#include <algorithm>
#include <vector>
#include <utility>

using namespace Collision;
using namespace DirectX;

namespace
{
    Vertex DebugVertex(const Vec3& p,const XMFLOAT4& color)
    {
        Vertex v{};
        v.Position={p.x,p.y,p.z};
        v.Normal={0.f,1.f,0.f};
        v.UV={0.f,0.f};
        v.Color=color;
        return v;
    }

    void AppendLine(std::vector<Vertex>& vertices,const Vec3& a,const Vec3& b,const XMFLOAT4& color)
    {
        vertices.push_back(DebugVertex(a,color));
        vertices.push_back(DebugVertex(b,color));
    }
}

bool CollisionDebugRenderer::Initialize(ID3D11Device* device,const std::filesystem::path& shaderDirectory)
{
    error_.clear();
    if(!device)
    {
        error_=L"CollisionDebugRenderer: ID3D11Device가 nullptr입니다.";
        return false;
    }
    device_=device;

    if(!shader_.Initialize(device,shaderDirectory/L"CollisionDebugVS.hlsl",
                                  shaderDirectory/L"CollisionDebugPS.hlsl"))
    {
        error_=shader_.GetLastErrorMessage();
        return false;
    }
    if(!transformBuffer_.Initialize(device))
    {
        error_=L"Collision Debug Transform ConstantBuffer 생성 실패";
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC depthDesc={};
    depthDesc.DepthEnable=TRUE;
    depthDesc.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
    depthDesc.StencilEnable=FALSE;
    if(FAILED(device->CreateDepthStencilState(&depthDesc,depthState_.GetAddressOf())))
    {
        error_=L"Collision Debug DepthStencilState 생성 실패";
        return false;
    }
    return true;
}

bool CollisionDebugRenderer::EnsureVertexCapacity(std::size_t vertexCount)
{
    if(vertexCount==0u)return true;
    if(vertexBuffer_&&vertexCapacity_>=vertexCount)return true;

    std::size_t capacity=256u;
    while(capacity<vertexCount)capacity*=2u;

    D3D11_BUFFER_DESC desc={};
    desc.ByteWidth=static_cast<UINT>(capacity*sizeof(Vertex));
    desc.Usage=D3D11_USAGE_DYNAMIC;
    desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;

    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    if(FAILED(device_->CreateBuffer(&desc,nullptr,buffer.GetAddressOf())))return false;
    vertexBuffer_=std::move(buffer);
    vertexCapacity_=capacity;
    return true;
}

void CollisionDebugRenderer::Render(ID3D11DeviceContext* context,
                                    const XMMATRIX& view,const XMMATRIX& projection,
                                    const CollisionWorld& world,ColliderId selectedId,
                                    bool highlightCollisions,bool showContacts)
{
    if(!context||!device_)return;

    std::vector<Vertex> vertices;
    vertices.reserve(world.Count()*160u+world.Contacts().size()*8u);

    const XMFLOAT4 normalColor={0.25f,0.82f,1.00f,1.f};
    const XMFLOAT4 triggerColor={0.85f,0.40f,1.00f,1.f};
    const XMFLOAT4 collisionColor={1.00f,0.25f,0.20f,1.f};
    const XMFLOAT4 selectedColor={1.00f,0.88f,0.18f,1.f};
    const XMFLOAT4 contactColor={0.25f,1.00f,0.45f,1.f};

    for(const auto& collider:world.Colliders())
    {
        if(!collider.Enabled)continue;
        XMFLOAT4 color=collider.IsTrigger?triggerColor:normalColor;
        if(highlightCollisions&&world.IsColliding(collider.Id))color=collisionColor;
        if(collider.Id==selectedId)color=selectedColor;
        CollisionGeometry::AppendWireframe(collider,color,vertices);
    }

    if(showContacts)
    {
        for(const auto& contact:world.Contacts())
        {
            const Vec3 p=contact.Point;
            const float crossSize=0.35f;
            AppendLine(vertices,p-Vec3{crossSize,0,0},p+Vec3{crossSize,0,0},contactColor);
            AppendLine(vertices,p-Vec3{0,crossSize,0},p+Vec3{0,crossSize,0},contactColor);
            AppendLine(vertices,p-Vec3{0,0,crossSize},p+Vec3{0,0,crossSize},contactColor);
            const float normalLength=std::max(0.8f,contact.PenetrationDepth+0.5f);
            AppendLine(vertices,p,p+contact.Normal*normalLength,contactColor);
        }
    }

    if(vertices.empty()||!EnsureVertexCapacity(vertices.size()))return;

    D3D11_MAPPED_SUBRESOURCE mapped={};
    if(FAILED(context->Map(vertexBuffer_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return;
    std::copy(vertices.begin(),vertices.end(),static_cast<Vertex*>(mapped.pData));
    context->Unmap(vertexBuffer_.Get(),0);

    CBTransform cb{};
    XMStoreFloat4x4(&cb.WorldViewProjection,XMMatrixTranspose(view*projection));
    transformBuffer_.Update(context,cb);
    transformBuffer_.BindVS(context,0);

    shader_.Bind(context);
    const UINT stride=sizeof(Vertex),offset=0;
    ID3D11Buffer* vb=vertexBuffer_.Get();
    context->IASetVertexBuffers(0,1,&vb,&stride,&offset);
    context->IASetIndexBuffer(nullptr,DXGI_FORMAT_UNKNOWN,0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> oldDepth;
    UINT oldStencilRef=0;
    context->OMGetDepthStencilState(oldDepth.GetAddressOf(),&oldStencilRef);
    context->OMSetDepthStencilState(depthState_.Get(),0);

    context->Draw(static_cast<UINT>(vertices.size()),0);
    context->OMSetDepthStencilState(oldDepth.Get(),oldStencilRef);
}
