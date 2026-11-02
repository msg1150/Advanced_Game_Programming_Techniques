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
    auto candidate=std::make_unique<RuntimeSession>();
    if(!candidate->Initialize(device,context,root,std::move(settings),std::move(ground))){status_=candidate->Status();return false;}
    candidate->Update(0,{},false);scenePath_=candidate->ScenePath();root_=root;
    session_=std::move(candidate);selected_={};playing_=false;componentIndex_=0;SelectNext();status_=session_->Status();return true;
}
bool EngineRuntimeFeature::Load(const std::filesystem::path& path)
{
    if(!session_)return false;
    if(!session_->Load(path)){status_=session_->Status();return false;}
    scenePath_=path;playing_=false;selected_={};status_=session_->Status();return true;
}
void EngineRuntimeFeature::Open(HWND owner)
{
    std::wstring error;const auto path=SceneFileDialogs::Choose(owner,false,error,{L"Engine Scene (*.scene)\0*.scene\0All files\0*.*\0\0",L"scene",L"Engine Scene 불러오기"});
    if(path)Load(*path);else if(!error.empty())status_=UIUtf8::FromWide(error);
}
void EngineRuntimeFeature::Save(HWND owner)
{
    if(!Current())return;
    std::wstring error;const auto path=SceneFileDialogs::Choose(owner,true,error,{L"Engine Scene (*.scene)\0*.scene\0All files\0*.*\0\0",L"scene",L"Engine Scene 저장",scenePath_.c_str()});
    if(!path){if(!error.empty())status_=UIUtf8::FromWide(error);return;}
    std::ostringstream bytes;
    if(!SceneSerializer::Write(bytes,*Current(),status_))return;
    if(!SceneFileDialogs::WriteReplacement(*path,bytes.str(),error)){status_=UIUtf8::FromWide(error);return;}
    scenePath_=*path;status_="Engine Scene saved";
}
GameObject* EngineRuntimeFeature::Selected()const
{return Current()?Current()->Resolve(selected_):nullptr;}
Component* EngineRuntimeFeature::SelectedComponent()const
{if(auto* object=Selected()){const auto c=object->Components();if(!c.empty())return c[componentIndex_%c.size()];}return nullptr;}
void EngineRuntimeFeature::SelectNext()
{
    auto* scene=Current();if(!scene)return;const auto objects=scene->Objects();if(objects.empty()){selected_={};return;}
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
            auto component=session_->Registry().Create(c->Type());component->Load(c->Save());component->SetEnabled(c->Enabled());
            component->RemapReferences({{source->Id(),copy.Id()}});
            copy.Add(std::move(component));
        }
        selected_=copy.Handle();status_="Object duplicated (local transform offset +1 X)";
    }
    catch(const std::exception& e){if(added)scene.Destroy(added);status_=e.what();}
}
void EngineRuntimeFeature::GroundAll()
{if(session_){session_->GroundAll();status_=session_->Status();}}
void EngineRuntimeFeature::AddComponent(const std::string& type,const Properties& properties)
{
    auto* object=Selected();if(!object){status_="Select a GameObject first";return;}
    try
    {
        auto component=session_->Registry().Create(type);component->Load(properties);object->Add(std::move(component));
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
    const auto* scene=Current();std::string text=(playing_?"PLAY":"PAUSE")+std::string(" / Objects: ")+std::to_string(scene?scene->Objects().size():0);
    if(session_)text+=" / Resources: "+std::to_string(session_->ResourceCount());
    if(session_&&!session_->ResourceError().empty())text+=" / "+session_->ResourceError();
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
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"리소스 캐시 비우기 / 재로드",[this]{if(session_)session_->ClearResources();}));
#endif
    ui.StartSecondColumn(panel);
    ui.AddToPanel(panel,std::make_unique<UILabel>([this]
    {if(auto* o=Selected())return UIUtf8::ToWide(o->Name+" #"+std::to_string(o->Id())+" Parent="+std::to_string(o->Parent()));return std::wstring(L"선택: 없음");}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"다음 GameObject 선택",[this]{SelectNext();}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"빈 GameObject 생성",[this]{if(auto* scene=Current())selected_=scene->Create("GameObject").Handle();}));
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
void EngineRuntimeFeature::Update(const Input& input,float dt,bool keyboardAvailable,const DirectX::XMFLOAT3& focus)
{
    if(!session_)return;const auto token=Current()?Current()->Token():0;Vec3 direction;
    if(playing_&&keyboardAvailable)direction={float(input.IsKeyDown('D'))-float(input.IsKeyDown('A')),0,float(input.IsKeyDown('W'))-float(input.IsKeyDown('S'))};
    session_->Update(dt,direction,playing_,focus);
    if(Current()&&Current()->Token()!=token){selected_={};SelectNext();}
}
void EngineRuntimeFeature::Render(ID3D11DeviceContext* context,const DirectX::XMMATRIX& view,const DirectX::XMMATRIX& projection,const DirectX::XMFLOAT3& cameraPosition)
{if(session_)session_->Render(context,view,projection,cameraPosition,debug_);}
