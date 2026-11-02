#include "Engine/Runtime/RuntimeSession.h"
#include "Engine/Project/Project.h"
#include "Core/IO/AtomicFile.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <chrono>
using Microsoft::WRL::ComPtr;
int wmain(int argc,wchar_t** argv)
{
    assert(argc==3);const auto source=std::filesystem::path(argv[1]);
    const auto root=std::filesystem::path(argv[2])/L"런타임 자원 & (한글)"/std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
    for(const wchar_t* folder:{L"모델",L"텍스처",L"셰이더/포함",L"셰이더/중첩",L"장면"})std::filesystem::create_directories(root/folder);
    std::filesystem::copy_file(source/L"Assets/Meshes/Cube.mesh",root/L"모델/상자.mesh");std::filesystem::copy_file(source/L"Assets/Textures/Terrain/Grass.png",root/L"텍스처/풀 색상.png");
    std::wstring error;
    assert(Files::WriteReplacement(root/L"셰이더/중첩/정점 상수.hlsli","cbuffer Transform:register(b0){float4x4 WVP;};\n",error));
    assert(Files::WriteReplacement(root/L"셰이더/포함/공통 상수.hlsli","#include \"../중첩/정점 상수.hlsli\"\n",error));
    std::ifstream vs(source/L"Shaders/Engine/ObjectVS.hlsl",std::ios::binary);std::string code((std::istreambuf_iterator<char>(vs)),{});const auto end=code.find('\n');code.replace(0,end,"#include \"포함/공통 상수.hlsli\"");
    assert(Files::WriteReplacement(root/L"셰이더/정점.hlsl",code,error));std::filesystem::copy_file(source/L"Shaders/Engine/ObjectPS.hlsl",root/L"셰이더/픽셀.hlsl");
    Engine::Scene scene("한글 Scene");auto& object=scene.Create("한글 상자");auto renderer=std::make_unique<Engine::MeshRendererComponent>();
    renderer->MeshAsset="모델/상자.mesh";renderer->TextureAsset="텍스처/풀 색상.png";renderer->VertexShader="셰이더/정점.hlsl";renderer->PixelShader="셰이더/픽셀.hlsl";object.Add(std::move(renderer));
    std::ostringstream bytes;std::string narrowError;assert(Engine::SceneSerializer::Write(bytes,scene,narrowError));const auto sceneFile=root/L"장면/시작.scene";assert(Files::WriteReplacement(sceneFile,bytes.str(),error));
    Engine::ProjectDefinition project;project.Name="한글 런타임";project.File=root/L"한글 런타임.dxproject";project.AssetRoot=root;project.StartupScene=sceneFile;
    assert(Engine::ProjectFile::Save(project,narrowError));Engine::ProjectDefinition loaded;assert(Engine::ProjectFile::Load(project.File,loaded,narrowError));
    assert(SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)));ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    assert(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,device.GetAddressOf(),nullptr,context.GetAddressOf())));
    D3D11_TEXTURE2D_DESC desc{};desc.Width=64;desc.Height=64;desc.ArraySize=1;desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> texture;ComPtr<ID3D11RenderTargetView> target;assert(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,texture.GetAddressOf())));assert(SUCCEEDED(device->CreateRenderTargetView(texture.Get(),nullptr,target.GetAddressOf())));
    auto* rtv=target.Get();context->OMSetRenderTargets(1,&rtv,nullptr);D3D11_VIEWPORT viewport{0,0,64,64,0,1};context->RSSetViewports(1,&viewport);
    Engine::RuntimeSession runtime;Engine::RuntimeSettings settings;settings.InitialScene=loaded.StartupScene;
    assert(runtime.Initialize(device.Get(),context.Get(),loaded.AssetRoot,settings));runtime.Update(0,{},false);const auto identity=DirectX::XMMatrixIdentity();runtime.Render(context.Get(),identity,identity);
    assert(runtime.ResourceError().empty()&&runtime.ResourceCount()==3&&runtime.Current()->Name()=="한글 Scene");
    context->ClearState();context->Flush();CoUninitialize();
}
