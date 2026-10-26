// Offscreen resource/HLSL integration test, never launches the runtime window.
#include "Engine/Graphics/GraphicsResources.h"
#include "Engine/Serialization/SceneSerializer.h"
#include <cassert>
#include <fstream>
using Microsoft::WRL::ComPtr;
int main()
{
    assert(SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)));
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    assert(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,device.GetAddressOf(),nullptr,context.GetAddressOf())));
    D3D11_TEXTURE2D_DESC desc{};desc.Width=64;desc.Height=64;desc.ArraySize=1;desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target;ComPtr<ID3D11RenderTargetView> view;
    assert(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,target.GetAddressOf())));assert(SUCCEEDED(device->CreateRenderTargetView(target.Get(),nullptr,view.GetAddressOf())));
    auto* rt=view.Get();context->OMSetRenderTargets(1,&rt,nullptr);D3D11_VIEWPORT viewport{0,0,64,64,0,1};context->RSSetViewports(1,&viewport);
    Engine::ComponentRegistry registry;Engine::RegisterGraphicsComponents(registry);
    std::ifstream input("Assets/Scenes/Runtime.engine.scene");std::unique_ptr<Engine::Scene> scene;std::string error;
    assert(Engine::SceneSerializer::Read(input,registry,scene,error));assert(scene->Objects().size()==5);
    Engine::GraphicsResources graphics;assert(graphics.Initialize(device.Get(),context.Get(),std::filesystem::current_path()));
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    const auto identity=DirectX::XMMatrixIdentity();graphics.Render(*scene,identity,identity);
    assert(graphics.Error().empty()&&graphics.ResourceCount()==2);
    D3D11_PRIMITIVE_TOPOLOGY topology{};context->IAGetPrimitiveTopology(&topology);assert(topology==D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    graphics.Render(*scene,identity,identity);assert(graphics.Error().empty()&&graphics.ResourceCount()==2);
    scene->Find(1)->Get<Engine::MeshRendererComponent>()->TextureAsset="Assets/Textures/Terrain/Grass.png";
    graphics.Render(*scene,identity,identity);assert(graphics.Error().empty()&&graphics.ResourceCount()==3);
    scene->Find(1)->Get<Engine::MeshRendererComponent>()->MeshAsset="Assets/Meshes/does-not-exist.mesh";
    graphics.Render(*scene,identity,identity);assert(!graphics.Error().empty());
    graphics.Clear();assert(graphics.ResourceCount()==0);context->ClearState();context->Flush();CoUninitialize();
}
