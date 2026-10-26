#pragma once
#include "Engine/Core/Scene.h"
namespace Engine
{
class MeshRendererComponent final:public Component
{
public:
    std::string MeshAsset,TextureAsset,VertexShader="Shaders/Engine/ObjectVS.hlsl",PixelShader="Shaders/Engine/ObjectPS.hlsl";
    Vec3 Tint{1,1,1};
    std::string Type()const override{return "MeshRenderer";}
    Properties Save()const override
    {Properties p{{"mesh",MeshAsset},{"texture",TextureAsset},{"vs",VertexShader},{"ps",PixelShader}};Put(p,"tint",Tint);return p;}
    void Load(const Properties& p)override
    {
        MeshAsset=Value(p,"mesh");TextureAsset=Value(p,"texture");VertexShader=Value(p,"vs",VertexShader);PixelShader=Value(p,"ps",PixelShader);Tint=Vector(p,"tint",{1,1,1});
        if(MeshAsset.empty()||VertexShader.empty()||PixelShader.empty())throw std::runtime_error("MeshRenderer requires mesh/vs/ps assets");
    }
};
inline void RegisterGraphicsComponents(ComponentRegistry& registry)
{registry.Register("MeshRenderer",[]{return std::make_unique<MeshRendererComponent>();});}
}
