// Offscreen WARP integration test; does not open or play the runtime application.
#include "Features/GPUPicking/GPUPickingSystem.h"
#include <cassert>
#include <chrono>
#include <thread>
#include <iostream>
using Microsoft::WRL::ComPtr;
using namespace DirectX;

int main(int argc,char** argv)
{
    assert(argc==2);ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    assert(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
        device.GetAddressOf(),nullptr,context.GetAddressOf())));
    GPUPickingSystem picking;
    if(!picking.Initialize(device.Get(),std::filesystem::path(argv[1])/L"GPUPicking"))
    {std::wcerr<<picking.GetError();return 1;}
    D3D11_TEXTURE2D_DESC desc{};desc.Width=64;desc.Height=64;desc.MipLevels=1;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depth;ComPtr<ID3D11DepthStencilView> dsv;
    assert(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,depth.GetAddressOf())));
    assert(SUCCEEDED(device->CreateDepthStencilView(depth.Get(),nullptr,dsv.GetAddressOf())));
    const XMMATRIX identity=XMMatrixIdentity();
    Collision::ColliderComponent nearCollider{1,L"nearCollider",{0,0,.3f},{},Collision::SphereShape{.15f}};
    Collision::ColliderComponent farCollider{2,L"farCollider",{0,0,.7f},{},Collision::SphereShape{.15f}};
    auto awaitResult=[&]()->GPUPickResult
    {
        context->Flush();const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(std::chrono::steady_clock::now()<deadline)
        {
            if(auto result=picking.Poll(context.Get()))return *result;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        assert(false&&"readback timed out");return {};
    };
    auto submit=[&](const std::vector<Collision::ColliderComponent>& colliders,float sceneDepth)
    {
        context->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,sceneDepth,0);
        context->OMSetRenderTargets(0,nullptr,dsv.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        assert(picking.Submit(context.Get(),dsv.Get(),identity,identity,colliders,32,32,64,64));
        D3D11_PRIMITIVE_TOPOLOGY topology{};context->IAGetPrimitiveTopology(&topology);
        assert(topology==D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        ComPtr<ID3D11DepthStencilView> restored;context->OMGetRenderTargets(0,nullptr,restored.GetAddressOf());assert(restored.Get()==dsv.Get());
        return awaitResult();
    };
    assert(submit({nearCollider,farCollider},1).ObjectId==1);assert(submit({farCollider,nearCollider},1).ObjectId==1);
    const auto hit=submit({nearCollider},1);assert(hit.Hit&&hit.Depth<.3f&&hit.WorldPosition.z<.3f);
    assert(!submit({nearCollider,farCollider},.1f).Hit);assert(!submit({},1).Hit);
    // Scene depth remains untouched by picking.
    desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.BindFlags=0;
    ComPtr<ID3D11Texture2D> staging;assert(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,staging.GetAddressOf())));
    context->OMSetRenderTargets(0,nullptr,nullptr);context->CopyResource(staging.Get(),depth.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};assert(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)));
    const auto* pixel=reinterpret_cast<const std::uint32_t*>(static_cast<const unsigned char*>(mapped.pData)+32*mapped.RowPitch)+32;
    assert((*pixel&0x00ffffffu)==0x00ffffffu);context->Unmap(staging.Get(),0);
    assert(picking.Submit(context.Get(),dsv.Get(),identity,identity,{nearCollider},32,32,64,64));
    picking.DiscardResults();context->Flush();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(picking.PendingCount()&&std::chrono::steady_clock::now()<deadline)
    {assert(!picking.Poll(context.Get()));std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(picking.PendingCount()==0);assert(submit({farCollider},1).ObjectId==2);
}

