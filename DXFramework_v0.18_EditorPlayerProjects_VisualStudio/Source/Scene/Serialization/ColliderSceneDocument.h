#pragma once
#include "Physics/Collision/Core/CollisionTypes.h"
#include <iosfwd>
#include <vector>

// Portable data format. No Runtime IDs, UI, dialogs, rendering or file paths.
// Runtime adapters assign fresh monotonic IDs when applying a document.
struct ColliderSceneDocument
{
    static constexpr unsigned Version=1;
    std::vector<Collision::ColliderComponent> Colliders;
};

class ColliderSceneSerializer
{
public:
    static bool Read(std::istream& input,ColliderSceneDocument& output,std::wstring& error);
    static bool Write(std::ostream& output,const ColliderSceneDocument& document,std::wstring& error);
};
