// ============================================================================
// GPUPickingSystem.cpp
// ============================================================================
#include "Features/GPUPicking/GPUPickingSystem.h"
#include "Graphics/Mesh.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include <utility>

using namespace Collision;
using namespace DirectX;

bool GPUPickingSystem::Initialize(ID3D11Device* device,const std::filesystem::path& shaderDirectory)
{
    error_.clear();
    if(!device)
    {
        error_=L"GPUPickingSystem: ID3D11Device가 nullptr입니다.";
        return false;
    }
    device_=device;

    if(!shader_.Initialize(device,shaderDirectory/L"PickingVS.hlsl",shaderDirectory/L"PickingPS.hlsl"))
    {
        error_=shader_.GetLastErrorMessage();
        return false;
    }
    if(!transformBuffer_.Initialize(device)||!objectBuffer_.Initialize(device))
    {
        error_=L"GPU Picking ConstantBuffer 생성 실패";
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC depthDesc={};
    depthDesc.DepthEnable=TRUE;
    depthDesc.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc=D3D11_COMPARISON_LESS;
    depthDesc.StencilEnable=FALSE;
    if(FAILED(device->CreateDepthStencilState(&depthDesc,pickingDepthState_.GetAddressOf())))
    {
        error_=L"GPU Picking DepthStencilState 생성 실패";
        return false;
    }

    // Collision Shape는 안쪽에서 Camera가 바라보는 경우에도 선택되어야 하므로
    // Picking Pass에서는 Back Face Culling을 사용하지 않는다.
    D3D11_RASTERIZER_DESC rs={};
    rs.FillMode=D3D11_FILL_SOLID;
    rs.CullMode=D3D11_CULL_NONE;
    rs.DepthClipEnable=TRUE;
    // Mouse Pixel 하나만 Rasterize한다. Vertex/Primitive 처리는 그대로지만
    // 불필요한 Pixel Shader/RenderTarget 쓰기를 화면 전체에서 수행하지 않는다.
    rs.ScissorEnable=TRUE;
    if(FAILED(device->CreateRasterizerState(&rs,rasterizerState_.GetAddressOf())))
    {
        error_=L"GPU Picking RasterizerState 생성 실패";
        return false;
    }

    if(!CreateReadbackSlots())return false;
    return true;
}

bool GPUPickingSystem::CopySceneDepth(ID3D11DeviceContext* context,ID3D11DepthStencilView* sceneDepth,UINT width,UINT height)
{
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;sceneDepth->GetResource(resource.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11Texture2D> source;
    if(FAILED(resource.As(&source)))return false;
    D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);
    if(desc.Width!=width||desc.Height!=height||desc.SampleDesc.Count!=1||desc.ArraySize!=1||desc.MipLevels!=1)
    {error_=L"Picking requires a single-sample 2D scene depth buffer.";return false;}
    D3D11_TEXTURE2D_DESC current{};if(pickingDepth_)pickingDepth_->GetDesc(&current);
    if(!pickingDepth_||!pickingDSV_||current.Width!=desc.Width||current.Height!=desc.Height||current.Format!=desc.Format)
    {
        pickingDSV_.Reset();pickingDepth_.Reset();
        desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=0;desc.MiscFlags=0;desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        D3D11_DEPTH_STENCIL_VIEW_DESC view{};sceneDepth->GetDesc(&view);view.Flags=0;
        if(FAILED(device_->CreateTexture2D(&desc,nullptr,pickingDepth_.GetAddressOf()))||
           FAILED(device_->CreateDepthStencilView(pickingDepth_.Get(),&view,pickingDSV_.GetAddressOf())))
        {error_=L"Picking private depth creation failed.";return false;}
    }
    // Unbind the source DSV while copying. The caller restores/binds its targets.
    context->OMSetRenderTargets(0,nullptr,nullptr);
    context->CopyResource(pickingDepth_.Get(),source.Get());return true;
}

bool GPUPickingSystem::CreateReadbackSlots()
{
    D3D11_TEXTURE2D_DESC idDesc={};
    idDesc.Width=1;idDesc.Height=1;idDesc.MipLevels=1;idDesc.ArraySize=1;
    idDesc.Format=DXGI_FORMAT_R32_UINT;idDesc.SampleDesc.Count=1;
    idDesc.Usage=D3D11_USAGE_STAGING;idDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;

    D3D11_TEXTURE2D_DESC depthDesc=idDesc;
    depthDesc.Format=DXGI_FORMAT_R32_FLOAT;

    D3D11_QUERY_DESC queryDesc={};
    queryDesc.Query=D3D11_QUERY_EVENT;

    for(auto& slot:slots_)
    {
        if(FAILED(device_->CreateTexture2D(&idDesc,nullptr,slot.IdStaging.GetAddressOf()))||
           FAILED(device_->CreateTexture2D(&depthDesc,nullptr,slot.DepthStaging.GetAddressOf()))||
           FAILED(device_->CreateQuery(&queryDesc,slot.CompletionQuery.GetAddressOf())))
        {
            error_=L"GPU Picking 1x1 Readback Ring 생성 실패";
            return false;
        }
    }
    return true;
}

bool GPUPickingSystem::EnsureTargets(UINT width,UINT height)
{
    if(width==0||height==0)return false;
    if(idTexture_&&depthTexture_&&targetWidth_==width&&targetHeight_==height)return true;

    idRTV_.Reset();idTexture_.Reset();depthRTV_.Reset();depthTexture_.Reset();

    D3D11_TEXTURE2D_DESC desc={};
    desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;
    desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET;

    desc.Format=DXGI_FORMAT_R32_UINT;
    if(FAILED(device_->CreateTexture2D(&desc,nullptr,idTexture_.GetAddressOf()))||
       FAILED(device_->CreateRenderTargetView(idTexture_.Get(),nullptr,idRTV_.GetAddressOf())))
    {
        error_=L"GPU Picking R32_UINT ID RenderTarget 생성 실패";
        return false;
    }

    desc.Format=DXGI_FORMAT_R32_FLOAT;
    if(FAILED(device_->CreateTexture2D(&desc,nullptr,depthTexture_.GetAddressOf()))||
       FAILED(device_->CreateRenderTargetView(depthTexture_.Get(),nullptr,depthRTV_.GetAddressOf())))
    {
        error_=L"GPU Picking R32_FLOAT Depth RenderTarget 생성 실패";
        return false;
    }

    targetWidth_=width;targetHeight_=height;
    return true;
}

bool GPUPickingSystem::EnsureVertexCapacity(std::size_t vertexCount)
{
    if(vertexCount==0u)return true;
    if(vertexBuffer_&&vertexCapacity_>=vertexCount)return true;

    std::size_t capacity=512u;
    while(capacity<vertexCount)capacity*=2u;

    D3D11_BUFFER_DESC desc={};
    desc.ByteWidth=static_cast<UINT>(capacity*sizeof(Vertex));
    desc.Usage=D3D11_USAGE_DYNAMIC;
    desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;

    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    if(FAILED(device_->CreateBuffer(&desc,nullptr,buffer.GetAddressOf())))
    {
        error_=L"GPU Picking Dynamic VertexBuffer 생성 실패";
        return false;
    }
    vertexBuffer_=std::move(buffer);
    vertexCapacity_=capacity;
    return true;
}

GPUPickingSystem::ReadbackSlot* GPUPickingSystem::AcquireFreeSlot()
{
    for(auto& slot:slots_)if(!slot.Pending)return &slot;
    return nullptr;
}

std::size_t GPUPickingSystem::PendingCount()const
{
    return static_cast<std::size_t>(std::count_if(slots_.begin(),slots_.end(),[](const ReadbackSlot& s){return s.Pending;}));
}

std::optional<GPUPickResult> GPUPickingSystem::Poll(ID3D11DeviceContext* context)
{
    if(!context)return std::nullopt;
    std::optional<GPUPickResult> newest;

    for(auto& slot:slots_)
    {
        if(!slot.Pending)continue;
        BOOL queryDone=FALSE;
        const HRESULT ready=context->GetData(slot.CompletionQuery.Get(),&queryDone,sizeof(queryDone),
                                             D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(ready!=S_OK||queryDone==FALSE)continue;

        D3D11_MAPPED_SUBRESOURCE idMapped={},depthMapped={};
        if(FAILED(context->Map(slot.IdStaging.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&idMapped)))continue;
        if(FAILED(context->Map(slot.DepthStaging.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&depthMapped)))
        {
            context->Unmap(slot.IdStaging.Get(),0);
            continue;
        }

        const std::uint32_t id=*static_cast<const std::uint32_t*>(idMapped.pData);
        const float depth=*static_cast<const float*>(depthMapped.pData);
        context->Unmap(slot.DepthStaging.Get(),0);
        context->Unmap(slot.IdStaging.Get(),0);

        GPUPickResult result;
        result.Hit=id!=0u;
        result.ObjectId=id;
        result.Depth=depth;
        result.Sequence=slot.Sequence;

        if(result.Hit&&slot.Width>0&&slot.Height>0)
        {
            const float ndcX=2.f*((static_cast<float>(slot.MouseX)+0.5f)/static_cast<float>(slot.Width))-1.f;
            const float ndcY=1.f-2.f*((static_cast<float>(slot.MouseY)+0.5f)/static_cast<float>(slot.Height));
            XMVECTOR clip=XMVectorSet(ndcX,ndcY,depth,1.f);
            const XMMATRIX inverse=XMLoadFloat4x4(&slot.InverseViewProjection);
            XMVECTOR world=XMVector4Transform(clip,inverse);
            const float w=XMVectorGetW(world);
            if(std::abs(w)>1.0e-6f)world=XMVectorScale(world,1.f/w);
            XMStoreFloat3(&result.WorldPosition,world);
        }

        slot.Pending=false;
        if(result.Sequence<minimumSequence_)continue;
        if(!newest||result.Sequence>newest->Sequence)newest=result;
    }
    if(newest)minimumSequence_=newest->Sequence+1;
    return newest;
}

bool GPUPickingSystem::Submit(ID3D11DeviceContext* context,ID3D11DepthStencilView* sceneDepth,
                              const XMMATRIX& view,const XMMATRIX& projection,
                              const std::vector<ColliderComponent>& colliders,
                              int mouseX,int mouseY,UINT width,UINT height)
{
    if(!device_||!context||!sceneDepth||mouseX<0||mouseY<0||
       mouseX>=static_cast<int>(width)||mouseY>=static_cast<int>(height))return false;
    if(!EnsureTargets(width,height))return false;
    ReadbackSlot* slot=AcquireFreeSlot();
    if(!slot)return false; // 3개의 비동기 Readback이 모두 처리 중이면 다음 Frame에 재시도.

    // 현재 메인 Render Target/State를 보존한다. Picking Feature가 제거되거나
    // 호출 위치가 바뀌어도 다른 Renderer의 Pipeline 상태를 가능한 한 침범하지 않는다.
    std::array<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> oldRTVs;
    std::array<ID3D11RenderTargetView*,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> oldTargets{};
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> oldDSV;
    context->OMGetRenderTargets(static_cast<UINT>(oldTargets.size()),oldTargets.data(),oldDSV.GetAddressOf());
    UINT oldTargetCount=0;
    for(UINT i=0;i<oldTargets.size();++i)
    {oldRTVs[i].Attach(oldTargets[i]);if(oldTargets[i])oldTargetCount=i+1;}
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> oldDepthState;
    UINT oldStencilRef=0;context->OMGetDepthStencilState(oldDepthState.GetAddressOf(),&oldStencilRef);
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> oldRS;
    context->RSGetState(oldRS.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11BlendState> oldBlendState;
    FLOAT oldBlendFactor[4]={};UINT oldSampleMask=0xFFFFFFFFu;
    context->OMGetBlendState(oldBlendState.GetAddressOf(),oldBlendFactor,&oldSampleMask);
    std::array<D3D11_VIEWPORT,D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> oldViewports{};
    UINT viewportCount=static_cast<UINT>(oldViewports.size());
    context->RSGetViewports(&viewportCount,oldViewports.data());
    std::array<D3D11_RECT,D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> oldScissors{};
    UINT oldScissorCount=static_cast<UINT>(oldScissors.size());
    context->RSGetScissorRects(&oldScissorCount,oldScissors.data());

    // Save every binding changed by Shader::Bind / this pass, so removing the
    // feature does not change the pipeline state observed by subsequent passes.
    Microsoft::WRL::ComPtr<ID3D11InputLayout> oldLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> oldVB,oldIB,oldVSCB,oldPSCB;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> oldVS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> oldPS;
    UINT oldStride=0,oldOffset=0,oldIndexOffset=0;DXGI_FORMAT oldIndexFormat{};
    D3D11_PRIMITIVE_TOPOLOGY oldTopology{};
    context->IAGetInputLayout(oldLayout.GetAddressOf());
    context->IAGetVertexBuffers(0,1,oldVB.GetAddressOf(),&oldStride,&oldOffset);
    context->IAGetIndexBuffer(oldIB.GetAddressOf(),&oldIndexFormat,&oldIndexOffset);
    context->IAGetPrimitiveTopology(&oldTopology);
    context->VSGetShader(oldVS.GetAddressOf(),nullptr,nullptr);context->PSGetShader(oldPS.GetAddressOf(),nullptr,nullptr);
    context->VSGetConstantBuffers(0,1,oldVSCB.GetAddressOf());context->PSGetConstantBuffers(0,1,oldPSCB.GetAddressOf());

    if(!CopySceneDepth(context,sceneDepth,width,height))return false;

    ID3D11RenderTargetView* targets[2]={idRTV_.Get(),depthRTV_.Get()};
    context->OMSetRenderTargets(2,targets,pickingDSV_.Get());
    // Integer ID Target은 Blend 대상이 아니므로 Picking Pass에서는 Blend를 명시적으로 끈다.
    context->OMSetBlendState(nullptr,nullptr,0xFFFFFFFFu);
    const float clearId[4]={0.f,0.f,0.f,0.f};
    const float clearDepth[4]={1.f,1.f,1.f,1.f};
    context->ClearRenderTargetView(idRTV_.Get(),clearId);
    context->ClearRenderTargetView(depthRTV_.Get(),clearDepth);
    context->OMSetDepthStencilState(pickingDepthState_.Get(),0);
    context->RSSetState(rasterizerState_.Get());
    const D3D11_VIEWPORT viewport={0.f,0.f,static_cast<float>(width),static_cast<float>(height),0.f,1.f};
    context->RSSetViewports(1,&viewport);
    const D3D11_RECT pickRect={mouseX,mouseY,mouseX+1,mouseY+1};
    context->RSSetScissorRects(1,&pickRect);

    shader_.Bind(context);
    CBTransform transform{};
    XMStoreFloat4x4(&transform.WorldViewProjection,XMMatrixTranspose(view*projection));
    transformBuffer_.Update(context,transform);
    transformBuffer_.BindVS(context,0);

    std::vector<Vertex> triangles;
    for(const auto& collider:colliders)
    {
        if(!collider.Enabled||collider.Id==0u)continue;
        CollisionGeometry::BuildSolidTriangles(collider,triangles);
        if(triangles.empty()||!EnsureVertexCapacity(triangles.size()))continue;

        D3D11_MAPPED_SUBRESOURCE mapped={};
        if(FAILED(context->Map(vertexBuffer_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))continue;
        std::copy(triangles.begin(),triangles.end(),static_cast<Vertex*>(mapped.pData));
        context->Unmap(vertexBuffer_.Get(),0);

        const UINT stride=sizeof(Vertex),offset=0;
        ID3D11Buffer* vb=vertexBuffer_.Get();
        context->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        context->IASetIndexBuffer(nullptr,DXGI_FORMAT_UNKNOWN,0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        CBPickObject object{};object.ObjectID=collider.Id;
        objectBuffer_.Update(context,object);
        objectBuffer_.BindPS(context,0);
        context->Draw(static_cast<UINT>(triangles.size()),0);
    }

    // 전체 Texture가 아니라 Mouse가 가리킨 1x1 Pixel만 Staging으로 복사한다.
    D3D11_BOX pixelBox={static_cast<UINT>(mouseX),static_cast<UINT>(mouseY),0,
                        static_cast<UINT>(mouseX+1),static_cast<UINT>(mouseY+1),1};
    context->CopySubresourceRegion(slot->IdStaging.Get(),0,0,0,0,idTexture_.Get(),0,&pixelBox);
    context->CopySubresourceRegion(slot->DepthStaging.Get(),0,0,0,0,depthTexture_.Get(),0,&pixelBox);

    const XMMATRIX inverse=XMMatrixInverse(nullptr,view*projection);
    XMStoreFloat4x4(&slot->InverseViewProjection,inverse);
    slot->MouseX=mouseX;slot->MouseY=mouseY;slot->Width=width;slot->Height=height;
    slot->Sequence=nextSequence_++;
    slot->Pending=true;
    context->End(slot->CompletionQuery.Get());

    // 메인 Scene Pipeline 복구.
    context->OMSetRenderTargets(oldTargetCount,oldTargets.data(),oldDSV.Get());
    context->OMSetBlendState(oldBlendState.Get(),oldBlendFactor,oldSampleMask);
    context->OMSetDepthStencilState(oldDepthState.Get(),oldStencilRef);
    context->RSSetState(oldRS.Get());
    context->RSSetViewports(viewportCount,oldViewports.data());
    context->RSSetScissorRects(oldScissorCount,oldScissors.data());
    context->IASetInputLayout(oldLayout.Get());
    ID3D11Buffer* vbRestore=oldVB.Get();context->IASetVertexBuffers(0,1,&vbRestore,&oldStride,&oldOffset);
    context->IASetIndexBuffer(oldIB.Get(),oldIndexFormat,oldIndexOffset);context->IASetPrimitiveTopology(oldTopology);
    context->VSSetShader(oldVS.Get(),nullptr,0);context->PSSetShader(oldPS.Get(),nullptr,0);
    ID3D11Buffer* vsRestore=oldVSCB.Get();context->VSSetConstantBuffers(0,1,&vsRestore);
    ID3D11Buffer* psRestore=oldPSCB.Get();context->PSSetConstantBuffers(0,1,&psRestore);
    return true;
}
