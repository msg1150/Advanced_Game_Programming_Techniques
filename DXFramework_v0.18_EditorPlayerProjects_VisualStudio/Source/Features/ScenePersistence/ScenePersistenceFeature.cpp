#include "Features/ScenePersistence/ScenePersistenceFeature.h"
#include "Features/CollisionEditor/CollisionScene.h"
#include "Scene/Runtime/ColliderSceneAdapter.h"
#include "Platform/SceneFileDialogs.h"
#include "UI/UIManager.h"
#include <fstream>
#include <sstream>

void ScenePersistenceFeature::Attach(UIManager& ui,CollisionScene& scene,HWND owner)
{
    const auto panel=ui.CreatePanel(L"Collider Scene 파일",L"Scene 파일 열기",L"Scene 파일 닫기",460.f,260.f);
    ui.AddToPanel(panel,std::make_unique<UILabel>([this]{return status_;}));
    ui.AddToPanel(panel,std::make_unique<UILabel>(L"저장 범위: Collider / Transform / Layer·Mask / Trigger"));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"Scene 저장...",[this,&scene,owner]{Save(scene,owner);}));
    ui.AddToPanel(panel,std::make_unique<UIButton>(L"Scene 불러오기...",[this,&scene,owner]{Load(scene,owner);}));
}
void ScenePersistenceFeature::Save(CollisionScene& scene,HWND owner)
{
    std::wstring error;const auto path=SceneFileDialogs::Choose(owner,true,error);
    if(!path){if(!error.empty())status_=error;return;}
    std::ostringstream bytes;
    if(!ColliderSceneSerializer::Write(bytes,ColliderSceneAdapter::Capture(scene.World()),error)||
       !SceneFileDialogs::WriteReplacement(*path,bytes.str(),error)){status_=error;return;}
    status_=L"저장 완료: "+path->filename().wstring();
}
void ScenePersistenceFeature::Load(CollisionScene& scene,HWND owner)
{
    std::wstring error;const auto path=SceneFileDialogs::Choose(owner,false,error);
    if(!path){if(!error.empty())status_=error;return;}
    std::error_code ec;const auto bytes=std::filesystem::file_size(*path,ec);
    if(ec||bytes>64u*1024u*1024u){status_=L"Scene 파일 크기 오류 (최대 64 MiB).";return;}
    std::ifstream in(*path,std::ios::binary);ColliderSceneDocument doc;
    if(!in||!ColliderSceneSerializer::Read(in,doc,error)){status_=error.empty()?L"Scene 파일 열기 실패":error;return;}
    try{ColliderSceneAdapter::Apply(doc,scene.World());}
    catch(const std::exception&){status_=L"Scene 메모리 준비 실패. 기존 Scene을 유지합니다.";return;}
    scene.ClearSelection();
    status_=L"불러오기 완료: "+path->filename().wstring();
}
