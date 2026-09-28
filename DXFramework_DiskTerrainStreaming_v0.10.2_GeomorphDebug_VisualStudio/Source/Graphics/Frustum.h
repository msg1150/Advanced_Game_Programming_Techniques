// ============================================================================
// Frustum.h
// ----------------------------------------------------------------------------
// Camera의 View / Projection Matrix를 이용해 World Space Frustum을 생성하고,
// BoundingBox가 화면에 포함되는지 검사하는 공용 Graphics 클래스.
//
// QuadTree 전용 클래스가 아니므로 Graphics 계층에 둔다.
// 이후 일반 Object Culling, Chunk Culling 등에서도 재사용할 수 있다.
// ============================================================================

#pragma once

#include <DirectXCollision.h>
#include <DirectXMath.h>

class Frustum
{
public:
    // 현재 Camera의 View / Projection Matrix를 이용해
    // World Space Frustum을 다시 계산한다.
    void Build(
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection);

    // BoundingBox가 Frustum과 어떤 관계인지 반환한다.
    //
    // DISJOINT   : 완전히 화면 밖
    // INTERSECTS : 일부가 화면 안
    // CONTAINS   : 완전히 화면 안
    DirectX::ContainmentType Contains(
        const DirectX::BoundingBox& worldBounds) const;

private:
    DirectX::BoundingFrustum worldFrustum_;
};
