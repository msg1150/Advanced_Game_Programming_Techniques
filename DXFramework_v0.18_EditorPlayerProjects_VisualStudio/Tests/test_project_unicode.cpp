#include "Engine/Project/Project.h"
#include "Core/IO/AtomicFile.h"
#include <cassert>
#include <fstream>
#include <chrono>
int wmain(int argc,wchar_t** argv)
{
    assert(argc==2);const auto base=std::filesystem::path(argv[1])/std::filesystem::path(L"한글 경로 & 공백 (검증)")/std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto source=base/L"원본 템플릿";std::filesystem::create_directories(source/L"Assets/장면");std::filesystem::create_directories(source/L"Shaders");
    std::wstring wideError;assert(Files::WriteReplacement(source/L"Assets/장면/시작.scene","DXENGINE_SCENE 1 \"한글\" 0\n",wideError));
    Engine::ProjectDefinition original;original.Name="템플릿";original.File=source/L"템플릿.dxproject";original.AssetRoot=source;original.StartupScene=source/L"Assets/장면/시작.scene";
    std::string error;assert(Engine::ProjectFile::Save(original,error));
    Engine::ProjectDefinition project;const auto destination=base/L"새 게임 日本語 😀 (A&B)";
    assert(Engine::ProjectManager::Create(destination,"새 게임 日本語 😀 (A&B)",original.File,project,error));
    assert(project.Name=="새 게임 日本語 😀 (A&B)"&&std::filesystem::is_regular_file(project.StartupScene));
    Engine::ProjectDefinition reopened;assert(Engine::ProjectFile::Load(project.File,reopened,error)&&reopened.File==project.File);
    Engine::RecentProjects recent(base/L"사용자 설정/최근 목록.utf8");assert(recent.Add(project.File));recent.Load();assert(recent.Entries().size()==1&&recent.Entries()[0]==project.File);
    // Metadata uses portable UTF-8 relative paths. Move the entire project and reopen.
    const auto moved=base/L"이동한 폴더 & 공백";const auto filename=project.File.filename();std::filesystem::rename(destination,moved);
    assert(Engine::ProjectFile::Load(moved/filename,reopened,error));assert(reopened.StartupScene==moved/L"Assets/장면/시작.scene");
    const auto preserved=reopened.File;assert(!Engine::ProjectManager::Create(moved,"덮어쓰기",original.File,reopened,error)&&reopened.File==preserved);
    assert(!Engine::ProjectManager::Create(base/L"실패","../탈출",original.File,reopened,error));
    assert(Files::WriteReplacement(base/L"손상.dxproject","DXPROJECT 1\n\"name\" \"오류\"\n\"assetRoot\" \"../밖\"\n\"startupScene\" \"a.scene\"\n",wideError));
    assert(!Engine::ProjectFile::Load(base/L"손상.dxproject",reopened,error)&&reopened.File==preserved);
    reopened.TerrainDirectory=moved/L"제외된 지형 모듈";assert(Engine::ProjectFile::Save(reopened,error));
    assert(Engine::ProjectFile::Load(reopened.File,reopened,error)); // optional module validates its own files
    const auto report=base/L"원자 저장.txt";assert(Files::WriteReplacement(report,"처음",wideError));assert(Files::WriteReplacement(report,"변경 내용",wideError));
    std::ifstream in(report,std::ios::binary);const std::string bytes((std::istreambuf_iterator<char>(in)),{});assert(bytes=="변경 내용");
}
