#pragma once
#include "Engine/Resources/ResourceCache.h"
#include "Engine/Graphics/MeshRendererComponent.h"
#include "Graphics/Mesh.h"
#include "Graphics/Shader.h"
#include "Graphics/Texture2D.h"
#include "Graphics/ConstantBuffer.h"

namespace Engine
{
class GraphicsResources
{
public:
    bool Initialize(ID3D11Device* device,ID3D11DeviceContext* context,std::filesystem::path root);
    void Render(const Scene& scene,const DirectX::XMMATRIX& view,const DirectX::XMMATRIX& projection);
    void Clear(){meshes_.Clear();shaders_.Clear();textures_.Clear();error_.clear();}
    std::size_t ResourceCount()const{return meshes_.Size()+shaders_.Size()+textures_.Size();}
    const std::string& Error()const{return error_;}
private:
    struct TransformCB{DirectX::XMFLOAT4X4 WVP;};
    struct MaterialCB{DirectX::XMFLOAT3 Tint;unsigned Textured;};
    std::shared_ptr<Mesh> MeshFor(const std::string& asset);
    std::shared_ptr<Shader> ShaderFor(const MeshRendererComponent& descriptor);
    std::shared_ptr<Texture2D> TextureFor(const std::string& asset);
    ID3D11Device* device_=nullptr;ID3D11DeviceContext* context_=nullptr;std::filesystem::path root_;
    ResourceCache<Mesh> meshes_;ResourceCache<Shader> shaders_;ResourceCache<Texture2D> textures_;
    ConstantBuffer<TransformCB> transform_;ConstantBuffer<MaterialCB> material_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    std::string error_;
};
}
