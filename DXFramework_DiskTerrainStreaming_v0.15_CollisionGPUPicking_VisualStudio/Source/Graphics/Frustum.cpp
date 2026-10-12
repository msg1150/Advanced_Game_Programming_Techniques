// ============================================================================
// Frustum.cpp
// ============================================================================

#include "Graphics/Frustum.h"

using namespace DirectX;

void Frustum::Build(
    const XMMATRIX& view,
    const XMMATRIX& projection)
{
    // ------------------------------------------------------------------------
    // Projection Matrix에서 View Space Frustum 형태를 만든다.
    // ------------------------------------------------------------------------
    BoundingFrustum viewFrustum;

    BoundingFrustum::CreateFromMatrix(
        viewFrustum,
        projection);

    // ------------------------------------------------------------------------
    // View Space -> World Space
    //
    // View Matrix의 역행렬은 Camera Local/View 공간을 World 공간으로 되돌린다.
    // ------------------------------------------------------------------------
    XMVECTOR determinant = {};

    const XMMATRIX inverseView =
        XMMatrixInverse(
            &determinant,
            view);

    viewFrustum.Transform(
        worldFrustum_,
        inverseView);
}

ContainmentType Frustum::Contains(
    const BoundingBox& worldBounds) const
{
    return worldFrustum_.Contains(
        worldBounds);
}
