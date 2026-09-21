// ============================================================================
// TextureSplatMaterial.h
// ----------------------------------------------------------------------------
// Terrain의 높이와 경사도를 이용해 자동 Texture Splatting을 수행하기 위한
// Material 리소스와 설정값을 관리한다.
//
// 현재 Layer:
// - Layer 0 : Grass
// - Layer 1 : Rock
// - Layer 2 : Snow
//
// 중요한 점:
// Splat Map을 사용하지 않는다.
// Texture Weight는 Pixel Shader에서 World Height와 World Normal을 이용해
// 매 Pixel마다 자동 계산한다.
//
// 이 클래스는 Terrain Geometry를 소유하지 않는다.
// Texture / Shader / Sampler / Material 설정만 담당한다.
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

struct TextureSplatMaterialDesc
{
    // ------------------------------------------------------------------------
    // Texture 경로
    // ------------------------------------------------------------------------
    std::filesystem::path Layer0TexturePath; // Grass
    std::filesystem::path Layer1TexturePath; // Rock
    std::filesystem::path Layer2TexturePath; // Snow

    // Terrain Texture 반복 횟수.
    float TextureTiling = 12.0f;

    // ------------------------------------------------------------------------
    // Height 기반 Grass -> Snow 전환
    //
    // GrassFadeStartHeight 아래에서는 기본적으로 Grass 비중이 높다.
    // GrassFadeEndHeight를 넘어가면 Grass 비중이 크게 줄어든다.
    // ------------------------------------------------------------------------
    float GrassFadeStartHeight = 0.6f;
    float GrassFadeEndHeight = 1.4f;

    // ------------------------------------------------------------------------
    // Slope 기반 Rock 전환
    //
    // Slope는 0.0 = 평지, 1.0 = 수직에 가까운 면이다.
    //
    // RockSlopeStart부터 Rock 비중이 증가하고,
    // RockSlopeEnd 이상에서는 Rock이 지배적인 Layer가 된다.
    // ------------------------------------------------------------------------
    float RockSlopeStart = 0.20f;
    float RockSlopeEnd = 0.55f;

    // ------------------------------------------------------------------------
    // Height 기반 Snow 전환
    //
    // SnowStartHeight부터 Snow가 나타나기 시작하고,
    // SnowFullHeight 이상에서는 평평한 고지대 기준 Snow가 최대 비중을 가진다.
    // ------------------------------------------------------------------------
    float SnowStartHeight = 1.3f;
    float SnowFullHeight = 2.4f;
};

class TextureSplatMaterial
{
public:
    bool Initialize(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        const std::filesystem::path& shaderDirectory,
        const TextureSplatMaterialDesc& desc);

    // Texture / Shader / Sampler / Material Constant Buffer를 바인딩한다.
    void Bind(
        ID3D11DeviceContext* context);

    // Feature가 사용한 Texture / Sampler Slot을 정리한다.
    void Unbind(
        ID3D11DeviceContext* context) const;

    const std::wstring& GetLastErrorMessage() const;

private:
    // HLSL의 CBMaterialSettings와 16 Byte 정렬을 정확히 맞춘다.
    struct CBMaterialSettings
    {
        // x = TextureTiling
        // y = GrassFadeStartHeight
        // z = GrassFadeEndHeight
        // w = RockSlopeStart
        DirectX::XMFLOAT4 Settings0;

        // x = RockSlopeEnd
        // y = SnowStartHeight
        // z = SnowFullHeight
        // w = Reserved
        DirectX::XMFLOAT4 Settings1;
    };

    bool CreateSampler(
        ID3D11Device* device);

private:
    Shader shader_;

    Texture2D layer0Texture_;
    Texture2D layer1Texture_;
    Texture2D layer2Texture_;

    // 모든 Terrain Texture는 반복해서 사용하므로 하나의 WRAP Sampler를 공유한다.
    Microsoft::WRL::ComPtr<ID3D11SamplerState> tiledSampler_;

    ConstantBuffer<CBMaterialSettings> materialBuffer_;
    CBMaterialSettings materialSettings_ = {};

    std::wstring lastErrorMessage_;
};
