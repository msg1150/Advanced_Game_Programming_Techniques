#pragma once
#include "Engine/Project/Project.h"
#include <Windows.h>
#include <functional>
#include <memory>
class ProjectWindow
{
public:
    ~ProjectWindow();
    bool Initialize(HINSTANCE instance,HWND owner,std::filesystem::path templateFile,
                    std::function<bool(const Engine::ProjectDefinition&)> open);
    void Show();void SetCurrent(const Engine::ProjectDefinition& project);
private:
    static LRESULT CALLBACK Procedure(HWND,UINT,WPARAM,LPARAM);
    LRESULT Message(UINT,WPARAM,LPARAM);void CreateProject();void ChooseProject();void OpenRecent();void RefreshRecent();
    void OpenFile(const std::filesystem::path&);void Status(const std::wstring&);
    HWND hwnd_=nullptr,owner_=nullptr,name_=nullptr,folder_=nullptr,recent_=nullptr,status_=nullptr;HINSTANCE instance_=nullptr;
    std::filesystem::path template_;std::unique_ptr<Engine::RecentProjects> recents_;
    std::function<bool(const Engine::ProjectDefinition&)> open_;
};
