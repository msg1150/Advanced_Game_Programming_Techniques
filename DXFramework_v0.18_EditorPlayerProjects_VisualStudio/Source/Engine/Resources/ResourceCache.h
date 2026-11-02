#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <filesystem>

namespace Engine
{
inline std::filesystem::path PathFromUtf8(const std::string& value)
{return std::filesystem::path(std::u8string(value.begin(),value.end()));}
// Single render-thread cache; strong handles keep resources alive independently
// of cache entries. Failures are retained until Invalidate/Clear, avoiding IO per frame.
template<class T>class ResourceCache
{
public:
    using Loader=std::function<std::shared_ptr<T>(std::string&)>;
    std::shared_ptr<T> Acquire(const std::string& key,const Loader& loader,std::string& error)
    {
        if(auto it=entries_.find(key);it!=entries_.end()){error=it->second.Error;return it->second.Value;}
        Entry entry;
        try{entry.Value=loader(entry.Error);if(!entry.Value&&entry.Error.empty())entry.Error="Resource loader returned no resource";}
        catch(const std::exception& e){entry.Error=e.what();}
        error=entry.Error;auto value=entry.Value;entries_.emplace(key,std::move(entry));return value;
    }
    void Invalidate(const std::string& key){entries_.erase(key);}
    void Clear(){entries_.clear();}
    void CollectUnused()
    {for(auto it=entries_.begin();it!=entries_.end();)if(it->second.Value&&it->second.Value.use_count()==1)it=entries_.erase(it);else ++it;}
    std::size_t Size()const{return entries_.size();}
private:
    struct Entry{std::shared_ptr<T> Value;std::string Error;};
    std::unordered_map<std::string,Entry> entries_;
};
inline std::string AssetKey(const std::filesystem::path& root,const std::string& relative)
{
    const auto path=(root/PathFromUtf8(relative)).lexically_normal();
    // weakly_canonical also resolves aliases/symlinks when files exist.
    std::error_code ec;auto canonical=std::filesystem::weakly_canonical(path,ec);
    const auto utf8=(ec?path:canonical).generic_u8string();
    return std::string(utf8.begin(),utf8.end());
}
}
