#pragma once
#include "Engine/Core/Scene.h"
#include <iosfwd>
namespace Engine
{
class SceneSerializer
{
public:
    static bool Write(std::ostream& output,const Scene& scene,std::string& error);
    // Transactional: output is replaced only after complete validation.
    // Missing modules become lossless MissingComponents, with warnings.
    static bool Read(std::istream& input,const ComponentRegistry& registry,std::unique_ptr<Scene>& output,
                     std::string& error,std::vector<std::string>* warnings=nullptr);
};
}
