// ============================================================================
// TriplanarMaterial.h
// ----------------------------------------------------------------------------
// Terrain의 World Position / World Normal을 이용해 X/Y/Z 세 방향에서
// Texture를 투영한 뒤 Normal 방향에 따라 섞는 Triplanar Material.
//
// 기존 TextureSplatMaterial은 수정하지 않는다.
// 이번 Material은 별도 Feature로 완전히 분리되어 있다.
//
// Layer 규칙은 이전 단계와 동일하게 유지한다.
// - 낮고 완만한 지형 -> Grass
// - 가파른 지형      -> Rock
// - 높고 완만한 지형 -> Snow
//
// 차이점:
// 기존 방식은 Terrain UV를 사용했지만,
// 이번 방식은 World Position을 이용해 X/Y/Z 방향으로 Texture를 투영한다.
// ============================================================================

#pragma once

#include "Graphics/ConstantBuffer.h"
#include "Graphics/Shader.h"
#include "Graphics/Texture2D.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>

struct TriplanarMaterialDesc
{
    // Terrain Layer Texture.
    std::filesystem::path GrassTexturePath;
    std::filesystem::path RockTexturePath;
    std::filesystem::path SnowTexturePath;

    // ------------------------------------------------------------------------
    // Triplanar Projection 설정
    // ------------------------------------------------------------------------

    // World Unit당 Texture 반복 배율.
    //
    // 현재 HeightMap Terrain 크기에서 약 2.0 정도면
    // 이전 Texture Splatting의 반복 밀도와 비슷하게 보인다.
    float ProjectionScale = 2.0f;

    // X/Y/Z 투영 Weight를 얼마나 날카롭게 구분할지 결정한다.
    //
    // 1.0:
    // 축 사이 Blend가 넓음.
    //
    // 4.0 이상:
    // Surface가 향하는 주축의 Texture가 더 강하게 선택됨.
    float BlendSharpness = 4.0f;

    // ------------------------------------------------------------------------
    // 이전 Height / Slope Texture Splatting 규칙을 그대로 유지한다.
    // ------------------------------------------------------------------------
    float GrassFadeStartHeight = 0.55f;
    float GrassFadeEndHeight = 1.35f;

    float RockSlopeStart = 0.18f;
    float RockSlopeEnd = 0.48f;

    float SnowStartHeight = 1.25f;
    float SnowFullHeight = 2.35f;
};

class TriplanarMaterial
{
public:
    bool Initialize(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        const std::filesystem::path& shaderDirectory,
        const TriplanarMaterialDesc& desc);

    void Bind(
        ID3D11DeviceContext* context);

    void Unbind(
        ID3D11DeviceContext* context) const;

    const std::wstring& GetLastErrorMessage() const;

private:
    // HLSL의 CBTriplanarSettings와 16 Byte 정렬을 맞춘다.
    struct CBTriplanarSettings
    {
        // x = ProjectionScale
        // y = BlendSharpness
        // z = GrassFadeStartHeight
        // w = GrassFadeEndHeight
        DirectX::XMFLOAT4 Settings0;

        // x = RockSlopeStart
        // y = RockSlopeEnd
        // z = SnowStartHeight
        // w = SnowFullHeight
        DirectX::XMFLOAT4 Settings1;
    };

    bool CreateSampler(
        ID3D11Device* device);

private:
    Shader shader_;

    Texture2D grassTexture_;
    Texture2D rockTexture_;
    Texture2D snowTexture_;

    Microsoft::WRL::ComPtr<ID3D11SamplerState> tiledSampler_;

    ConstantBuffer<CBTriplanarSettings> settingsBuffer_;
    CBTriplanarSettings settings_ = {};

    std::wstring lastErrorMessage_;
};
