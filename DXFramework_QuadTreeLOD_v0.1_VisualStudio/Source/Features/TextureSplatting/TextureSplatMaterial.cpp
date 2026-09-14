// ============================================================================
// TextureSplatMaterial.cpp
// ============================================================================

#include "Features/TextureSplatting/TextureSplatMaterial.h"

#include <sstream>

bool TextureSplatMaterial::Initialize(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::filesystem::path& shaderDirectory,
    const TextureSplatMaterialDesc& desc)
{
    lastErrorMessage_.clear();

    if (!device ||
        !context)
    {
        lastErrorMessage_ =
            L"TextureSplatMaterial에 전달된 Device 또는 Context가 nullptr입니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 설정값 검증
    // ------------------------------------------------------------------------
    if (desc.TextureTiling <= 0.0f)
    {
        lastErrorMessage_ =
            L"TextureTiling은 0보다 커야 합니다.";

        return false;
    }

    if (desc.GrassFadeEndHeight <=
        desc.GrassFadeStartHeight)
    {
        lastErrorMessage_ =
            L"GrassFadeEndHeight는 GrassFadeStartHeight보다 커야 합니다.";

        return false;
    }

    if (desc.RockSlopeEnd <=
        desc.RockSlopeStart)
    {
        lastErrorMessage_ =
            L"RockSlopeEnd는 RockSlopeStart보다 커야 합니다.";

        return false;
    }

    if (desc.SnowFullHeight <=
        desc.SnowStartHeight)
    {
        lastErrorMessage_ =
            L"SnowFullHeight는 SnowStartHeight보다 커야 합니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 1. 자동 Texture Splatting Shader 초기화
    // ------------------------------------------------------------------------
    if (!shader_.Initialize(
            device,
            shaderDirectory / L"TextureSplatVS.hlsl",
            shaderDirectory / L"TextureSplatPS.hlsl"))
    {
        std::wstringstream stream;

        stream
            << L"Texture Splatting Shader 초기화 실패\n\n"
            << L"Shader Directory:\n"
            << shaderDirectory.wstring()
            << L"\n\n"
            << shader_.GetLastErrorMessage();

        lastErrorMessage_ =
            stream.str();

        return false;
    }

    // ------------------------------------------------------------------------
    // 2. Terrain Layer Texture 로드
    //
    // Grass / Rock / Snow는 실제 색상 Texture이므로 SRGB로 로드한다.
    // ------------------------------------------------------------------------
    if (!layer0Texture_.Load(
            device,
            context,
            desc.Layer0TexturePath,
            TextureColorSpace::SRGB))
    {
        lastErrorMessage_ =
            L"Grass Texture 로드 실패\n" +
            layer0Texture_.GetLastErrorMessage();

        return false;
    }

    if (!layer1Texture_.Load(
            device,
            context,
            desc.Layer1TexturePath,
            TextureColorSpace::SRGB))
    {
        lastErrorMessage_ =
            L"Rock Texture 로드 실패\n" +
            layer1Texture_.GetLastErrorMessage();

        return false;
    }

    if (!layer2Texture_.Load(
            device,
            context,
            desc.Layer2TexturePath,
            TextureColorSpace::SRGB))
    {
        lastErrorMessage_ =
            L"Snow Texture 로드 실패\n" +
            layer2Texture_.GetLastErrorMessage();

        return false;
    }

    // ------------------------------------------------------------------------
    // 3. 반복 Texture용 Sampler 생성
    // ------------------------------------------------------------------------
    if (!CreateSampler(device))
    {
        lastErrorMessage_ =
            L"Texture Splatting Sampler State 생성에 실패했습니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 4. Material Constant Buffer 생성
    // ------------------------------------------------------------------------
    if (!materialBuffer_.Initialize(device))
    {
        lastErrorMessage_ =
            L"Texture Splatting Material Constant Buffer 생성에 실패했습니다.";

        return false;
    }

    // C++ 설정값을 HLSL Constant Buffer 형식으로 보관한다.
    materialSettings_.Settings0 =
    {
        desc.TextureTiling,
        desc.GrassFadeStartHeight,
        desc.GrassFadeEndHeight,
        desc.RockSlopeStart
    };

    materialSettings_.Settings1 =
    {
        desc.RockSlopeEnd,
        desc.SnowStartHeight,
        desc.SnowFullHeight,
        0.0f
    };

    return true;
}

void TextureSplatMaterial::Bind(
    ID3D11DeviceContext* context)
{
    shader_.Bind(context);

    // ------------------------------------------------------------------------
    // Texture Slot
    //
    // t0: Grass
    // t1: Rock
    // t2: Snow
    //
    // 이전 버전의 t3 Splat Map은 제거했다.
    // ------------------------------------------------------------------------
    layer0Texture_.BindPS(
        context,
        0u);

    layer1Texture_.BindPS(
        context,
        1u);

    layer2Texture_.BindPS(
        context,
        2u);

    ID3D11SamplerState* sampler =
        tiledSampler_.Get();

    context->PSSetSamplers(
        0u,
        1u,
        &sampler);

    materialBuffer_.Update(
        context,
        materialSettings_);

    // b1:
    // b0은 TextureSplatTerrain의 Transform Constant Buffer가 사용한다.
    materialBuffer_.BindPS(
        context,
        1u);
}

void TextureSplatMaterial::Unbind(
    ID3D11DeviceContext* context) const
{
    Texture2D::UnbindPS(
        context,
        0u);

    Texture2D::UnbindPS(
        context,
        1u);

    Texture2D::UnbindPS(
        context,
        2u);

    ID3D11SamplerState* nullSampler = nullptr;

    context->PSSetSamplers(
        0u,
        1u,
        &nullSampler);
}

const std::wstring& TextureSplatMaterial::GetLastErrorMessage() const
{
    return lastErrorMessage_;
}

bool TextureSplatMaterial::CreateSampler(
    ID3D11Device* device)
{
    D3D11_SAMPLER_DESC samplerDesc = {};

    samplerDesc.Filter =
        D3D11_FILTER_MIN_MAG_MIP_LINEAR;

    // Terrain Texture는 TextureTiling에 의해 UV가 1.0을 넘으므로
    // 반복되는 WRAP 방식을 사용한다.
    samplerDesc.AddressU =
        D3D11_TEXTURE_ADDRESS_WRAP;

    samplerDesc.AddressV =
        D3D11_TEXTURE_ADDRESS_WRAP;

    samplerDesc.AddressW =
        D3D11_TEXTURE_ADDRESS_WRAP;

    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    samplerDesc.ComparisonFunc =
        D3D11_COMPARISON_NEVER;

    const HRESULT hr =
        device->CreateSamplerState(
            &samplerDesc,
            tiledSampler_.GetAddressOf());

    return SUCCEEDED(hr);
}
