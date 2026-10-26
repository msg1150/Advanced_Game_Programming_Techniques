#pragma once
#include "Core/IO/Paths.h"
#include <vector>

namespace Engine
{
struct ProjectDefinition
{
    std::string Name;
    std::filesystem::path File,AssetRoot,StartupScene,TerrainDirectory;
    std::vector<std::string> ContentDirectories{"Assets","Shaders"};
};
class ProjectFile
{
public:
    static bool Load(const std::filesystem::path& file,ProjectDefinition& output,std::string& error,bool validateFiles=true);
    static bool Save(const ProjectDefinition& project,std::string& error);
};
class ProjectManager
{
public:
    // Never overwrites an existing directory; template includes data directories only.
    static bool Create(const std::filesystem::path& directory,const std::string& name,
                       const std::filesystem::path& templateFile,ProjectDefinition& output,std::string& error);
};
class RecentProjects
{
public:
    explicit RecentProjects(std::filesystem::path file):file_(std::move(file)){}
    void Load();
    bool Add(const std::filesystem::path& projectFile);
    const std::vector<std::filesystem::path>& Entries()const{return entries_;}
private:
    std::filesystem::path file_;std::vector<std::filesystem::path> entries_;
};
}
