#pragma once
#include "Features/EngineRuntime/EngineRuntimeFeature.h"
#include "Editor/ProjectWindow.h"
#include "Platform/UnicodeProcess.h"
#include "Platform/Window.h"
#include "Graphics/Renderer.h"
#include "Graphics/Camera.h"
#include "Graphics/CameraController.h"
#include "Input/Input.h"
#include "Core/Timer.h"
#include "UI/UIManager.h"
#include "UI/UIRenderer.h"
class EditorApplication
{
public:
    bool Initialize(HINSTANCE instance,const std::filesystem::path& initialProject);
    int Run();const std::wstring& Error()const{return error_;}
private:
    bool OpenProject(const Engine::ProjectDefinition&);void LaunchPlayer();void SaveProject();
    Window window_;Input input_;Timer timer_;Renderer renderer_;Camera camera_;CameraController controller_;
    UIRenderer uiRenderer_;UIManager ui_;EngineRuntimeFeature runtime_;ProjectWindow projects_;Native::ChildProcess player_;
    Engine::ProjectDefinition project_;std::filesystem::path base_;std::wstring error_,status_;
};
