#include "Engine/Project/Project.h"
#include <fstream>
#include <sstream>
#include "Core/IO/AtomicFile.h"
#include <iomanip>
#include <map>
#include <algorithm>
#include <atomic>

namespace Engine
{
namespace
{
bool NameValid(const std::string& name)
{
    if(name.empty()||!Paths::ValidUtf8(name)||name.size()>1024||name=="."||name==".."||name.back()=='.'||name.back()==' ')return false;
    for(unsigned char ch:name)if(ch<32)return false;
    return name.find_first_of("<>:\"/\\|?*\r\n")==std::string::npos;
}
std::string Relative(const std::filesystem::path& root,const std::filesystem::path& path)
{
    const auto normalized=Paths::Absolute(path);
    if(!Paths::IsWithin(root,normalized))throw std::runtime_error("Project member is outside the project");
    return Paths::ToUtf8(normalized.lexically_relative(Paths::Absolute(root)));
}
}
bool ProjectFile::Load(const std::filesystem::path& file,ProjectDefinition& output,std::string& error,bool validateFiles)
{
    error.clear();try
    {
        if(std::filesystem::file_size(file)>1024u*1024u)throw std::runtime_error("Project file exceeds 1 MiB");
        std::ifstream in(file,std::ios::binary);std::string magic;unsigned version=0;
        if(!(in>>magic>>version)||magic!="DXPROJECT"||version!=1)throw std::runtime_error("Unsupported project format/version");
        std::map<std::string,std::string> fields;in>>std::ws;
        while(in.peek()!=std::char_traits<char>::eof())
        {
            std::string key,value;if(in.peek()!='"'||!(in>>std::quoted(key)>>std::ws)||in.peek()!='"'||!(in>>std::quoted(value))||
               key.size()>65536||value.size()>65536||!Paths::ValidUtf8(key)||!Paths::ValidUtf8(value)||fields.size()>=256||!fields.emplace(key,value).second)
                throw std::runtime_error("Invalid/duplicate UTF-8 project field");
            in>>std::ws;
        }
        for(const char* required:{"name","assetRoot","startupScene"})if(!fields.contains(required))throw std::runtime_error("Required project field missing");
        ProjectDefinition p;p.File=Paths::Absolute(file);p.Name=fields.at("name");if(!NameValid(p.Name))throw std::runtime_error("Invalid project name");
        const auto root=p.File.parent_path();p.AssetRoot=Paths::RelativeMember(root,fields.at("assetRoot"));p.StartupScene=Paths::RelativeMember(root,fields.at("startupScene"));
        if(fields.contains("terrain")&&!fields.at("terrain").empty())p.TerrainDirectory=Paths::RelativeMember(root,fields.at("terrain"));
        p.ContentDirectories.clear();
        for(unsigned i=0;fields.contains("content."+std::to_string(i));++i)
        {const auto& entry=fields.at("content."+std::to_string(i));Paths::RelativeMember(root,entry);if(Paths::FromUtf8(entry).lexically_normal()==".")throw std::runtime_error("Content directory cannot be project root");p.ContentDirectories.push_back(entry);}
        if(p.ContentDirectories.empty())p.ContentDirectories={"Assets","Shaders"};
        // Optional module data is validated by that module, not by project IO.
        // A terrain-disabled Player can reopen a project even without tile files.
        if(validateFiles&&(!std::filesystem::is_directory(p.AssetRoot)||!std::filesystem::is_regular_file(p.StartupScene)))throw std::runtime_error("Project content/startup scene is missing");
        output=std::move(p);return true;
    }
    catch(const std::exception& e){error=e.what();return false;}
}
bool ProjectFile::Save(const ProjectDefinition& p,std::string& error)
{
    error.clear();try
    {
        if(!NameValid(p.Name))throw std::runtime_error("Invalid project name");
        const auto root=Paths::Absolute(p.File).parent_path();
        std::map<std::string,std::string> fields{{"name",p.Name},{"assetRoot",Relative(root,p.AssetRoot)},{"startupScene",Relative(root,p.StartupScene)},
            {"terrain",p.TerrainDirectory.empty()?"":Relative(root,p.TerrainDirectory)}};
        for(std::size_t i=0;i<p.ContentDirectories.size();++i){Paths::RelativeMember(root,p.ContentDirectories[i]);fields["content."+std::to_string(i)]=p.ContentDirectories[i];}
        std::ostringstream out;out<<"DXPROJECT 1\n";
        for(const auto& [key,value]:fields)out<<std::quoted(key)<<' '<<std::quoted(value)<<'\n';std::wstring wideError;
        if(!Files::WriteReplacement(p.File,out.str(),wideError))throw std::runtime_error(TextUtf8::FromWide(wideError));return true;
    }
    catch(const std::exception& e){error=e.what();return false;}
}
bool ProjectManager::Create(const std::filesystem::path& directory,const std::string& name,const std::filesystem::path& templateFile,ProjectDefinition& output,std::string& error)
{
    error.clear();try
    {
        if(!NameValid(name))throw std::runtime_error("Invalid project name");
        const auto destination=Paths::Absolute(directory);if(std::filesystem::exists(destination))throw std::runtime_error("Project directory already exists");
        ProjectDefinition source;if(!ProjectFile::Load(templateFile,source,error))return false;
        std::filesystem::create_directories(destination.parent_path());
        static std::atomic<unsigned> counter{0};std::filesystem::path staging;
        for(unsigned attempt=0;attempt<100;++attempt)
        {staging=destination;staging+=L".creating."+std::to_wstring(++counter);if(std::filesystem::create_directory(staging))break;staging.clear();}
        if(staging.empty())throw std::runtime_error("Cannot create project staging directory");
        const auto sourceRoot=source.File.parent_path();
        for(const auto& content:source.ContentDirectories)
        {
            const auto from=Paths::RelativeMember(sourceRoot,content),to=Paths::RelativeMember(staging,content);
            if(Paths::IsWithin(from,staging))throw std::runtime_error("Destination is inside template content");
            std::filesystem::create_directories(to.parent_path());
            std::filesystem::copy(from,to,std::filesystem::copy_options::recursive);
        }
        std::filesystem::create_directories(staging/L"Source");std::filesystem::create_directories(staging/L"Build");
        ProjectDefinition created=source;created.Name=name;auto projectName=Paths::FromUtf8(name);projectName+=L".dxproject";created.File=staging/projectName;
        created.AssetRoot=staging/source.AssetRoot.lexically_relative(sourceRoot);
        created.StartupScene=staging/source.StartupScene.lexically_relative(sourceRoot);
        if(!source.TerrainDirectory.empty())created.TerrainDirectory=staging/source.TerrainDirectory.lexically_relative(sourceRoot);
        if(!ProjectFile::Save(created,error))return false;
        const auto filename=created.File.filename();std::filesystem::rename(staging,destination);
        return ProjectFile::Load(destination/filename,output,error);
    }
    catch(const std::exception& e){error=e.what();return false;}
}
void RecentProjects::Load()
{
    entries_.clear();try
    {
        if(!std::filesystem::exists(file_)||std::filesystem::file_size(file_)>1024*1024)return;
        std::ifstream in(file_,std::ios::binary);std::string line;
        while(entries_.size()<12&&std::getline(in,line))if(!line.empty()&&Paths::ValidUtf8(line))
        {auto path=Paths::FromUtf8(line);if(std::filesystem::is_regular_file(path))entries_.push_back(std::move(path));}
    }catch(...){entries_.clear();}
}
bool RecentProjects::Add(const std::filesystem::path& file)
{
    try
    {
        auto path=Paths::Absolute(file);entries_.erase(std::remove(entries_.begin(),entries_.end(),path),entries_.end());entries_.insert(entries_.begin(),path);
        if(entries_.size()>12)entries_.resize(12);std::filesystem::create_directories(file_.parent_path());
        std::ostringstream out;for(const auto& p:entries_)out<<Paths::ToUtf8(p)<<'\n';std::wstring error;return Files::WriteReplacement(file_,out.str(),error);
    }catch(...){return false;}
}
}
