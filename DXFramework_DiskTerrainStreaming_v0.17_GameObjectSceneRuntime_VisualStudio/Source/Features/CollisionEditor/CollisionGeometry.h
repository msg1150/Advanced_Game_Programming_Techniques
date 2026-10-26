// ============================================================================
// CollisionGeometry.h
// ----------------------------------------------------------------------------
// Collision Shape의 수학적 결과를 D3D11 Debug/Picking용 Vertex로 변환한다.
// Collision Core는 Graphics를 모르고, 이 Adapter만 Graphics::Vertex를 사용한다.
// ============================================================================
#pragma once

#include "Graphics/Mesh.h"
#include "Physics/Collision/Core/CollisionTypes.h"

#include <DirectXMath.h>
#include <vector>

namespace CollisionGeometry
{
    void AppendWireframe(const Collision::ColliderComponent& collider,
                         const DirectX::XMFLOAT4& color,
                         std::vector<Vertex>& outVertices);

    void BuildSolidTriangles(const Collision::ColliderComponent& collider,
                             std::vector<Vertex>& outVertices);
}
