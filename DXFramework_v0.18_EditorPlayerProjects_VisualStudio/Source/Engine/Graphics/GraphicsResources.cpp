#include "Engine/Graphics/GraphicsResources.h"
#include "Core/Text/Utf8.h"
#include <fstream>

namespace Engine
{
using Microsoft::WRL::ComPtr;
namespace
{
// Save only the bindings this pass changes; no unrelated renderer dependency.
struct PipelineScope
{
    ID3D11DeviceContext* C;ComPtr<ID3D11InputLayout> Layout;ComPtr<ID3D11Buffer> VB,IB,VSCB,PSCB;
    ComPtr<ID3D11VertexShader> VS;ComPtr<ID3D11PixelShader> PS;ComPtr<ID3D11ShaderResourceView> SRV;
    ComPtr<ID3D11SamplerState> Sampler;ComPtr<ID3D11DepthStencilState> Depth;ComPtr<ID3D11BlendState> Blend;
    ComPtr<ID3D11RasterizerState> Raster;UINT Stride=0,Offset=0,IndexOffset=0,Stencil=0,Mask=0;DXGI_FORMAT Format{};
    D3D11_PRIMITIVE_TOPOLOGY Topology{};FLOAT Factor[4]{};
    explicit PipelineScope(ID3D11DeviceContext* c):C(c)
    {
        C->IAGetInputLayout(Layout.GetAddressOf());C->IAGetVertexBuffers(0,1,VB.GetAddressOf(),&Stride,&Offset);C->IAGetIndexBuffer(IB.GetAddressOf(),&Format,&IndexOffset);C->IAGetPrimitiveTopology(&Topology);
        C->VSGetShader(VS.GetAddressOf(),nullptr,nullptr);C->PSGetShader(PS.GetAddressOf(),nullptr,nullptr);
        C->VSGetConstantBuffers(0,1,VSCB.GetAddressOf());C->PSGetConstantBuffers(0,1,PSCB.GetAddressOf());
        C->PSGetShaderResources(0,1,SRV.GetAddressOf());C->PSGetSamplers(0,1,Sampler.GetAddressOf());
        C->OMGetDepthStencilState(Depth.GetAddressOf(),&Stencil);C->OMGetBlendState(Blend.GetAddressOf(),Factor,&Mask);C->RSGetState(Raster.GetAddressOf());
    }
    ~PipelineScope()
    {
        C->IASetInputLayout(Layout.Get());auto* vb=VB.Get();C->IASetVertexBuffers(0,1,&vb,&Stride,&Offset);C->IASetIndexBuffer(IB.Get(),Format,IndexOffset);C->IASetPrimitiveTopology(Topology);
        C->VSSetShader(VS.Get(),nullptr,0);C->PSSetShader(PS.Get(),nullptr,0);auto* v=VSCB.Get();C->VSSetConstantBuffers(0,1,&v);auto* p=PSCB.Get();C->PSSetConstantBuffers(0,1,&p);
        auto* srv=SRV.Get();C->PSSetShaderResources(0,1,&srv);auto* sampler=Sampler.Get();C->PSSetSamplers(0,1,&sampler);
        C->OMSetDepthStencilState(Depth.Get(),Stencil);C->OMSetBlendState(Blend.Get(),Factor,Mask);C->RSSetState(Raster.Get());
    }
};
}
bool GraphicsResources::Initialize(ID3D11Device* device,ID3D11DeviceContext* context,std::filesystem::path root)
{
    device_=device;context_=context;root_=std::move(root);if(!device||!context)return false;
    D3D11_SAMPLER_DESC desc{};desc.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;desc.AddressU=desc.AddressV=desc.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;desc.MaxLOD=D3D11_FLOAT32_MAX;
    return transform_.Initialize(device)&&material_.Initialize(device)&&SUCCEEDED(device->CreateSamplerState(&desc,sampler_.GetAddressOf()));
}
std::shared_ptr<Mesh> GraphicsResources::MeshFor(const std::string& asset)
{
    const auto key=AssetKey(root_,asset);
    return meshes_.Acquire(key,[&](std::string& error)->std::shared_ptr<Mesh>
    {
        std::ifstream in(PathFromUtf8(key));in.imbue(std::locale::classic());std::string magic;unsigned version=0;std::size_t vc=0,ic=0;
        if(!(in>>magic>>version>>vc>>ic)||magic!="DXMESH"||version!=1||!vc||vc>1000000||!ic||ic>3000000||ic%3)throw std::runtime_error("Invalid mesh asset: "+asset);
        std::vector<Vertex> vertices(vc);std::vector<std::uint32_t> indices(ic);
        for(auto& v:vertices)
        {
            if(!(in>>v.Position.x>>v.Position.y>>v.Position.z>>v.Normal.x>>v.Normal.y>>v.Normal.z>>v.UV.x>>v.UV.y>>v.Color.x>>v.Color.y>>v.Color.z>>v.Color.w))throw std::runtime_error("Invalid vertex: "+asset);
            for(float value:{v.Position.x,v.Position.y,v.Position.z,v.Normal.x,v.Normal.y,v.Normal.z,v.UV.x,v.UV.y,v.Color.x,v.Color.y,v.Color.z,v.Color.w})if(!std::isfinite(value))throw std::runtime_error("Non-finite vertex");
        }
        for(auto& index:indices)if(!(in>>index)||index>=vc)throw std::runtime_error("Invalid mesh index");
        in>>std::ws;if(!in.eof())throw std::runtime_error("Trailing mesh asset data");
        auto mesh=std::make_shared<Mesh>();if(!mesh->Initialize(device_,vertices,indices)){error="Mesh GPU upload failed: "+asset;return {};}
        return mesh;
    },error_);
}
std::shared_ptr<Shader> GraphicsResources::ShaderFor(const MeshRendererComponent& d)
{
    const auto vs=AssetKey(root_,d.VertexShader),ps=AssetKey(root_,d.PixelShader);
    return shaders_.Acquire(vs+"\n"+ps,[&](std::string& error)->std::shared_ptr<Shader>
    {auto shader=std::make_shared<Shader>();if(!shader->Initialize(device_,PathFromUtf8(vs),PathFromUtf8(ps))){error=TextUtf8::FromWide(shader->GetLastErrorMessage());return {};}return shader;},error_);
}
std::shared_ptr<Texture2D> GraphicsResources::TextureFor(const std::string& asset)
{
    const auto key=AssetKey(root_,asset)+"|srgb";
    return textures_.Acquire(key,[&](std::string& error)->std::shared_ptr<Texture2D>
    {auto texture=std::make_shared<Texture2D>();if(!texture->Load(device_,context_,PathFromUtf8(AssetKey(root_,asset)),TextureColorSpace::SRGB)){error=TextUtf8::FromWide(texture->GetLastErrorMessage());return {};}return texture;},error_);
}
void GraphicsResources::Render(const Scene& scene,const DirectX::XMMATRIX& view,const DirectX::XMMATRIX& projection)
{
    using namespace DirectX;PipelineScope state(context_);std::string frameError;
    context_->OMSetDepthStencilState(nullptr,0);context_->OMSetBlendState(nullptr,nullptr,0xffffffffu);context_->RSSetState(nullptr);
    auto* sampler=sampler_.Get();context_->PSSetSamplers(0,1,&sampler);
    for(auto* object:scene.Objects())if(object->Active())if(auto* renderer=object->Get<MeshRendererComponent>();renderer&&renderer->Enabled())
    {
        auto mesh=MeshFor(renderer->MeshAsset);if(!mesh){frameError=error_;continue;}
        auto shader=ShaderFor(*renderer);if(!shader){frameError=error_;continue;}
        std::shared_ptr<Texture2D> texture;
        if(!renderer->TextureAsset.empty()){texture=TextureFor(renderer->TextureAsset);if(!texture){frameError=error_;continue;}}
        const auto m=object->WorldMatrix();
        const XMMATRIX world(m.X.x,m.X.y,m.X.z,0,m.Y.x,m.Y.y,m.Y.z,0,m.Z.x,m.Z.y,m.Z.z,0,m.T.x,m.T.y,m.T.z,1);
        TransformCB cb;XMStoreFloat4x4(&cb.WVP,XMMatrixTranspose(world*view*projection));transform_.Update(context_,cb);transform_.BindVS(context_,0);
        material_.Update(context_,{{renderer->Tint.x,renderer->Tint.y,renderer->Tint.z},texture?1u:0u});material_.BindPS(context_,0);
        if(texture)texture->BindPS(context_,0);else Texture2D::UnbindPS(context_,0);
        shader->Bind(context_);mesh->Bind(context_);mesh->Draw(context_);
    }
    error_=std::move(frameError);
}
}
