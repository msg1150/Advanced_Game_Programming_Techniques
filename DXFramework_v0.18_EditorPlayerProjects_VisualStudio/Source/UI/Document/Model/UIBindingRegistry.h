// ============================================================================
// UIBindingRegistry.h
// ----------------------------------------------------------------------------
// XML 문서의 문자열 Binding 이름과 실제 C++ Getter/Setter/Action을 연결합니다.
// UI 문서는 Terrain/Game 클래스를 include하지 않고, 게임 쪽에서 필요한 값만 등록합니다.
// Unreal MVVM / QML Property Binding / RmlUi Data Model과 같은 역할 경계를 참고했습니다.
// ============================================================================
#pragma once
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
class UIBindingRegistry
{
public:
    struct BoolBinding
    {
        std::function<bool()> Get;
        std::function<void(bool)> Set;
    };
    struct FloatBinding
    {
        std::function<float()> Get;
        std::function<void(float)> Set;
    };
    using TextBinding=std::function<std::wstring()>;
    using ActionBinding=std::function<void()>;

    void Clear(){bools_.clear();floats_.clear();texts_.clear();actions_.clear();}
    bool RegisterBool(std::string name,std::function<bool()> getter,std::function<void(bool)> setter)
    {return bools_.emplace(std::move(name),BoolBinding{std::move(getter),std::move(setter)}).second;}
    bool RegisterFloat(std::string name,std::function<float()> getter,std::function<void(float)> setter)
    {return floats_.emplace(std::move(name),FloatBinding{std::move(getter),std::move(setter)}).second;}
    bool RegisterText(std::string name,TextBinding getter)
    {return texts_.emplace(std::move(name),std::move(getter)).second;}
    bool RegisterAction(std::string name,ActionBinding action)
    {return actions_.emplace(std::move(name),std::move(action)).second;}

    const BoolBinding* FindBool(std::string_view name)const{return Find(bools_,name);}
    const FloatBinding* FindFloat(std::string_view name)const{return Find(floats_,name);}
    const TextBinding* FindText(std::string_view name)const{return Find(texts_,name);}
    const ActionBinding* FindAction(std::string_view name)const{return Find(actions_,name);}
private:
    template<class Map>
    static const typename Map::mapped_type* Find(const Map& map,std::string_view name)
    {
        const auto it=map.find(std::string(name));
        return it==map.end()?nullptr:&it->second;
    }
    std::unordered_map<std::string,BoolBinding> bools_;
    std::unordered_map<std::string,FloatBinding> floats_;
    std::unordered_map<std::string,TextBinding> texts_;
    std::unordered_map<std::string,ActionBinding> actions_;
};
