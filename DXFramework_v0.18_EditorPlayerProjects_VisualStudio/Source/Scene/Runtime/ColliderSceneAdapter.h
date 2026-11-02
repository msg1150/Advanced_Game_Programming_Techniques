#pragma once
#include "Scene/Serialization/ColliderSceneDocument.h"
#include "Physics/Collision/Core/CollisionWorld.h"

namespace ColliderSceneAdapter
{
    ColliderSceneDocument Capture(const Collision::CollisionWorld& world);
    // Build a candidate world first, preserving the runtime's ID allocator.
    // No existing object changes until construction of the new world succeeds.
    void Apply(const ColliderSceneDocument& document,Collision::CollisionWorld& world);
}
