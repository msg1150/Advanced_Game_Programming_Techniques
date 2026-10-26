#pragma once
#include <Windows.h>
#include <string>
class UIManager;
class CollisionScene;

// Optional UI/platform adapter. Serialization and runtime data are portable.
class ScenePersistenceFeature
{
public:
    void Attach(UIManager& ui,CollisionScene& scene,HWND owner);
private:
    void Save(CollisionScene& scene,HWND owner);
    void Load(CollisionScene& scene,HWND owner);
    std::wstring status_=L"Collider Scene 파일을 저장하거나 불러오세요.";
};
