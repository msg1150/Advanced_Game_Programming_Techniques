#include "Features/EngineRuntime/EngineRuntimeFeature.h"
#include "Engine/Components/ObjectLink.h"
#if DX_ENABLE_ENGINE_GAMEPLAY
#include "Engine/Gameplay/Movement.h"
#endif
#include "Platform/SceneFileDialogs.h"
#include "UI/UIManager.h"
#include "UI/Document/Serialization/UIUtf8.h"
#include "Input/Input.h"
#include <fstream>

using namespace Engine;
bool EngineRuntimeFeature::Initialize(ID3D11Device* device,ID3D11DeviceContext* context,const std::filesystem::path& root,EngineRuntimeSettings settings,GroundProvider ground)
{
    settings_=std::move(settings);ground_=std::move(ground);RegisterCoreComponents(registry_);
#if DX_ENABLE_ENGINE_GRAPHICS
    RegisterGraphicsComponents(registry_);graphicsReady_=resources_.Initialize(device,context,root);
#endif
#if DX_ENABLE_ENGINE_COLLISION
    RegisterCollisionComponents(registry_);debugReady_=debugRenderer_.Initialize(device,root/L"Shaders/CollisionDebug");
#endif
#if DX_ENABLE_ENGINE_GAMEPLAY
    RegisterGameplayComponents(registry_,[this](GameObject& object,Vec3 displacement)
    {
#if DX_ENABLE_ENGINE_COLLISION
        collision_.Move(object,displacement,ground_,settings_.GroundClearance);
#else
        auto next=object.WorldMatrix().T+displacement;
        if(ground_){float height;Vec3 normal;if(!ground_(next.x,next.z,height,normal))return;next.y=height+settings_.GroundClearance;}
        object.SetWorldPosition(next);
#endif
    });
#endif
    (void)device;(void)context;(void)root;
    return Load(settings_.InitialScene);
}
bool EngineRuntimeFeature::Load(const std::filesystem::path& path)
{
    std::error_code ec;const auto size=std::filesystem::file_size(path,ec);
    if(ec||size>16u*1024u*1024u){status_="Engine Scene file is unavailable or exceeds 16 MiB";return false;}
    std::ifstream in(path,std::ios::binary);std::unique_ptr<Scene> scene;std::vector<std::string> warnings;
    if(!SceneSerializer::Read(in,registry_,scene,status_,&warnings))return false;
#if DX_ENABLE_ENGINE_COLLISION
    scene->AfterFixedUpdate=[this](Scene& s,float){collision_.DetectAndDispatch(s);};
#endif
    scenes_.Request(std::move(scene));scenePath_=path;playing_=false;selected_={};
    status_=warnings.empty()?"Scene loaded":"Scene loaded; missing components preserved: "+std::to_string(warnings.size());return true;
}
void EngineRuntimeFeature::Open(HWND owner)
{
    std::wstring error;const auto path=SceneFileDialogs::Choose(owner,false,error,{L"Engine Scene (*.scene)\0*.scene\0All files\0*.*\0\0",L"scene",L"Engine Scene 불러오기"});
    if(path)Load(*path);else if(!error.empty())status_=UIUtf8::FromWide(error);
}
void EngineRuntimeFeature::Save(HWND owner)
{
    if(!scenes_.Current())return;
    std::wstring error;const auto path=SceneFileDialogs::Choose(owner,true,error,{L"Engine Scene (*.scene)\0*.scene\0All files\0*.*\0\0",L"scene",L"Engine Scene 저장"});
    if(!path){if(!error.empty())status_=UIUtf8::FromWide(error);return;}
    std::ostringstream bytes;
    if(!SceneSerializer::Write(bytes,*scenes_.Current(),status_))return;
    if(!SceneFileDialogs::WriteReplacement(*path,bytes.str(),error)){status_=UIUtf8::FromWide(error);return;}
    scenePath_=*path;status_="Engine Scene saved";
}
GameObject* EngineRuntimeFeature::Selected()const
{return scenes_.Current()?scenes_.Current()->Resolve(selected_):nullptr;}
Component* EngineRuntimeFeature::SelectedComponent()const
{if(auto* object=Selected()){const auto c=object->Components();if(!c.empty())return c[componentIndex_%c.size()];}return nullptr;}
void EngineRuntimeFeature::SelectNext()
{
    auto* scene=scenes_.Current();if(!scene)return;const auto objects=scene->Objects();if(objects.empty()){selected_={};return;}
    auto it=std::find_if(objects.begin(),objects.end(),[this](auto* o){return o->Handle()==selected_;});
    if(it==objects.end()||++it==objects.end())it=objects.begin();selected_=(*it)->Handle();componentIndex_=0;
}
void EngineRuntimeFeature::DuplicateSelected()
{
    auto* source=Selected();if(!source)return;auto& scene=source->World();ObjectId added=0;
    try
    {
        auto& copy=scene.Create(source->Name+" Copy");added=copy.Id();copy.Local=source->Local;copy.Local.Position.x+=1;copy.SetEnabled(source->Enabled());copy.SetParent(source->Parent());
        for(auto* c:source->Components())
        {
            auto component=registry_.Create(c->Type());component->Load(c->Save());component->SetEnabled(c->Enabled());
            component->RemapReferences({{source->Id(),copy.Id()}});
            copy.Add(std::move(component));
        }
        selected_=copy.Handle();status_="Object duplicated (local transform offset +1 X)";
    }
    catch(const std::exception& e){if(added)scene.Destroy(added);status_=e.what();}
}
void EngineRuntimeFeature::GroundAll()
{
    if(!scenes_.Current()||!ground_){status_="No ground provider attached";return;}
    for(auto* object:scenes_.Current()->Objects())
    {
#if DX_ENABLE_ENGINE_COLLISION
        if(object->Get<CollisionBody>())collision_.Move(*object,{},ground_,settings_.GroundClearance);
#else
        float height;Vec3 normal;auto p=object->WorldMatrix().T;if(ground_(p.x,p.z,height,normal)){p.y=height+settings_.GroundClearance;object->SetWorldPosition(p);}
#endif
    }
    status_="Ground placement requested";
}
void EngineRuntimeFeature::AddComponent(const std::string& type,const Properties& properties)
{
    auto* object=Selected();if(!object){status_="Select a GameObject first";return;}
    try
    {
        auto component=registry_.Create(type);component->Load(properties);object->Add(std::move(component));
        status_="Component attached: "+type;
    }
    catch(const std::exception& e){status_=e.what();}
}
float EngineRuntimeFeature::GetTransform(unsigned field)const
{
    auto* object=Selected();if(!object)return field>=6?1.f:0.f;
    const auto v=field<3?object->Local.Position:field<6?object->Local.Rotation:object->Local.Scale;
    const float value=field%3==0?v.x:field%3==1?v.y:v.z;
    return field>=3&&field<6?Collision::RadiansToDegrees(value):value;
}
void EngineRuntimeFeature::SetTransform(unsigned field,float value)
{
    auto* object=Selected();if(!object||!std::isfinite(value))return;
    if(field>=3&&field<6)value=Collision::DegreesToRadians(value);
    auto& v=field<3?object->Local.Position:field<6?object->Local.Rotation:object->Local.Scale;
    if(field>=6)value=std::max(.01f,value);
    if(field%3==0)v.x=value;else if(field%3==1)v.y=value;else v.z=value;
}
std::wstring EngineRuntimeFeature::Status()const
{
    const auto* scene=scenes_.Current();std::string text=(playing_?"PLAY":"PAUSE")+std::string(" / Objects: ")+std::to_string(scene?scene->Objects().size():0);
#if DX_ENABLE_ENGINE_GRAPHICS
    text+=" / Resources: "+std::to_string(resources_.ResourceCount());if(!resources_.Error().empty())text+=" / "+resources_.Error();
#endif
    if(scene&&!scene->LastError().empty())text+=" / "+scene->LastError();return UIUtf8::ToWide(text);
}
void EngineRuntimeFeature::Attach(UIManager& ui,HWND owner)
{
    const auto panel=ui.CreatePanel(L"GameObject / Scene Runtime",L"Engine 열기",L"Engine 닫기",600,650);
    ui.AddToPanel(panel,std::make_unique<UILabel>([this]{return Status();}));
    ui.AddToPanel(panel,std::make_unique<UILabel>([this]{return UIUtf8::ToWide(status_);}));
    ui.AddToPanel(panel,std::make_unique<UICheckBox>(L"Play (WASD: Movement Component)",[this]{return playing_;},[this](bool v){playing_=v;}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"Engine Scene 저장...",[this,owner]{Save(owner);}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"Engine Scene 불러오기...",[this,owner]{Open(owner);}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"현재 Scene 재진입 (파일에서 복원)",[this]{Load(scenePath_);}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"모두 지면에 맞추기",[this]{GroundAll();}));
#if DX_ENABLE_ENGINE_COLLISION
    ui.AddToPanel(panel,std::make_unique<UICheckBox>(L"Engine Collider / Contact 보기",[this]{return debug_;},[this](bool v){debug_=v;}));
#endif
#if DX_ENABLE_ENGINE_GRAPHICS
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"리소스 캐시 비우기 / 재로드",[this]{resources_.Clear();}));
#endif
    ui.StartSecondColumn(panel);
    ui.AddToPanel(panel,std::make_unique<UILabel>([this]
    {if(auto* o=Selected())return UIUtf8::ToWide(o->Name+" #"+std::to_string(o->Id())+" Parent="+std::to_string(o->Parent()));return std::wstring(L"선택: 없음");}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"다음 GameObject 선택",[this]{SelectNext();}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"빈 GameObject 생성",[this]{if(auto* scene=scenes_.Current())selected_=scene->Create("GameObject").Handle();}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"선택 GameObject 복제",[this]{DuplicateSelected();}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"선택 GameObject 삭제 (자식 포함)",[this]{if(auto* o=Selected())o->World().Destroy(o->Id());selected_={};}));
    ui.AddToPanel(panel,std::make_unique<UICheckBox>(L"GameObject 활성",[this]{auto* o=Selected();return o&&o->Enabled();},[this](bool v){if(auto* o=Selected())o->SetEnabled(v);}));
    const wchar_t* labels[]={L"Local Position X",L"Local Position Y",L"Local Position Z",L"Local Rotation X",L"Local Rotation Y",L"Local Rotation Z",L"Local Scale X",L"Local Scale Y",L"Local Scale Z"};
    for(unsigned i=0;i<9;++i)ui.AddToPanel(panel,std::make_unique<UISlider>(labels[i],i<3?-256.f:i<6?-180.f:.01f,i<3?256.f:i<6?180.f:20.f,i<3?.1f:i<6?1.f:.1f,[this,i]{return GetTransform(i);},[this,i](float v){SetTransform(i,v);}));
    ui.AddToPanel(panel,std::make_unique<UILabel>([this]{auto* c=SelectedComponent();return UIUtf8::ToWide(c?"Component: "+c->Type():"Component: none");}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"다음 Component",[this]{++componentIndex_;}));
    ui.AddToPanel(panel,std::make_unique<UICheckBox>(L"Component 활성",[this]{auto* c=SelectedComponent();return c&&c->Enabled();},[this](bool v){if(auto* c=SelectedComponent())c->SetEnabled(v);}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"선택 Component 제거",[this]{if(auto* c=SelectedComponent())c->Owner().Remove(c->Type());}));
#if DX_ENABLE_ENGINE_GRAPHICS
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"MeshRenderer 붙이기 (Mesh 선택)...",[this,owner]
    {
        if(!Selected()){status_="Select a GameObject first";return;}
        std::wstring error;const auto path=SceneFileDialogs::Choose(owner,false,error,{L"Static Mesh (*.mesh)\0*.mesh\0All files\0*.*\0\0",L"mesh",L"Mesh Asset 선택"});
        if(path)AddComponent("MeshRenderer",{{"mesh",UIUtf8::FromWide(path->wstring())}});else if(!error.empty())status_=UIUtf8::FromWide(error);
    }));
#endif
#if DX_ENABLE_ENGINE_COLLISION
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"Collider 붙이기",[this]{AddComponent("Collider");}));
#endif
#if DX_ENABLE_ENGINE_GAMEPLAY
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"Movement 붙이기",[this]{AddComponent("Movement");}));
#endif
}
void EngineRuntimeFeature::Update(const Input& input,float dt,bool keyboardAvailable)
{
    const auto token=scenes_.Current()?scenes_.Current()->Token():0;
#if DX_ENABLE_ENGINE_GAMEPLAY
    if(auto* scene=scenes_.Current())for(auto* object:scene->Objects())if(auto* movement=object->Get<Movement>())
    {
        movement->InputDirection={};
        if(playing_&&keyboardAvailable)movement->InputDirection={float(input.IsKeyDown('D'))-float(input.IsKeyDown('A')),0,float(input.IsKeyDown('W'))-float(input.IsKeyDown('S'))};
    }
#endif
    (void)input;(void)keyboardAvailable;scenes_.Update(playing_?dt:0);
    if(scenes_.Current()&&scenes_.Current()->Token()!=token){selected_={};SelectNext();}
    if(auto* scene=scenes_.Current())
    {
        scene->Refresh();
#if DX_ENABLE_ENGINE_COLLISION
        collision_.Synchronize(*scene);
#endif
    }
}
void EngineRuntimeFeature::Render(ID3D11DeviceContext* context,const DirectX::XMMATRIX& view,const DirectX::XMMATRIX& projection)
{
    auto* scene=scenes_.Current();if(!scene)return;
#if DX_ENABLE_ENGINE_GRAPHICS
    if(graphicsReady_)resources_.Render(*scene,view,projection);
#endif
#if DX_ENABLE_ENGINE_COLLISION
    if(debug_&&debugReady_)debugRenderer_.Render(context,view,projection,collision_.World(),0,true,true);
#endif
    (void)context;(void)view;(void)projection;
}
