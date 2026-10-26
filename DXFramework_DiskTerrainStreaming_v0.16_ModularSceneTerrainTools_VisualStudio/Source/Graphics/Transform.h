// ============================================================================
// Transform.h
// ----------------------------------------------------------------------------
// 모든 3D Object에서 공통으로 사용할 World Transform 구조.
//
// Position / Rotation / Scale을 가지고
// 최종 World Matrix를 생성한다.
//
// Terrain, 일반 Mesh, Model 등 앞으로 추가할 Render Object들도
// 같은 Transform 구조를 재사용할 수 있다.
// ============================================================================

#pragma once

#include <DirectXMath.h>

struct Transform
{
    DirectX::XMFLOAT3 Position =
    {
        0.0f,
        0.0f,
        0.0f
    };

    // Radian 단위의 Pitch(X), Yaw(Y), Roll(Z).
    DirectX::XMFLOAT3 Rotation =
    {
        0.0f,
        0.0f,
        0.0f
    };

    DirectX::XMFLOAT3 Scale =
    {
        1.0f,
        1.0f,
        1.0f
    };

    DirectX::XMMATRIX GetWorldMatrix() const
    {
        // Object Local 크기.
        const DirectX::XMMATRIX scaleMatrix =
            DirectX::XMMatrixScaling(
                Scale.x,
                Scale.y,
                Scale.z);

        // Object Local 회전.
        const DirectX::XMMATRIX rotationMatrix =
            DirectX::XMMatrixRotationRollPitchYaw(
                Rotation.x,
                Rotation.y,
                Rotation.z);

        // World 위치.
        const DirectX::XMMATRIX translationMatrix =
            DirectX::XMMatrixTranslation(
                Position.x,
                Position.y,
                Position.z);

        // DirectXMath의 Row Vector Convention에 맞춰
        // Scale -> Rotation -> Translation 순으로 적용한다.
        return
            scaleMatrix *
            rotationMatrix *
            translationMatrix;
    }
};
