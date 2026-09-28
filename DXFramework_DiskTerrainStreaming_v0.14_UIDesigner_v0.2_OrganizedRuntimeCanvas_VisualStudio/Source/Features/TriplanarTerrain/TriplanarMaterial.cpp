// ============================================================================
// TriplanarMaterial.cpp
// ============================================================================

#include "Features/TriplanarTerrain/TriplanarMaterial.h"

#include <sstream>

bool TriplanarMaterial::Initialize(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::filesystem::path& shaderDirectory,
    const TriplanarMaterialDesc& desc)
{
    lastErrorMessage_.clear();

    if (!device ||
        !context)
    {
        lastErrorMessage_ =
            L"TriplanarMaterial에 전달된 Device 또는 Context가 nullptr입니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 설정값 검증
    // ------------------------------------------------------------------------
    if (desc.ProjectionScale <= 0.0f)
    {
        lastErrorMessage_ =
            L"ProjectionScale은 0보다 커야 합니다.";

        return false;
    }

    if (desc.BlendSharpness <= 0.0f)
    {
        lastErrorMessage_ =
            L"BlendSharpness는 0보다 커야 합니다.";

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
    // 1. Triplanar 전용 Shader
    // ------------------------------------------------------------------------
    if (!shader_.Initialize(
            device,
            shaderDirectory / L"TriplanarVS.hlsl",
            shaderDirectory / L"TriplanarPS.hlsl"))
    {
        std::wstringstream stream;

        stream
            << L"Triplanar Shader 초기화 실패\n\n"
            << L"Shader Directory:\n"
            << shaderDirectory.wstring()
            << L"\n\n"
            << shader_.GetLastErrorMessage();

        lastErrorMessage_ =
            stream.str();

        return false;
    }

    // ------------------------------------------------------------------------
    // 2. 기존 Terrain Texture 재사용
    //
    // 모두 색상 Texture이므로 SRGB로 로드한다.
    // ------------------------------------------------------------------------
    if (!grassTexture_.Load(
            device,
            context,
            desc.GrassTexturePath,
            TextureColorSpace::SRGB))
    {
        lastErrorMessage_ =
            L"Triplanar Grass Texture 로드 실패\n" +
            grassTexture_.GetLastErrorMessage();

        return false;
    }

    if (!rockTexture_.Load(
            device,
            context,
            desc.RockTexturePath,
            TextureColorSpace::SRGB))
    {
        lastErrorMessage_ =
            L"Triplanar Rock Texture 로드 실패\n" +
            rockTexture_.GetLastErrorMessage();

        return false;
    }

    if (!snowTexture_.Load(
            device,
            context,
            desc.SnowTexturePath,
            TextureColorSpace::SRGB))
    {
        lastErrorMessage_ =
            L"Triplanar Snow Texture 로드 실패\n" +
            snowTexture_.GetLastErrorMessage();

        return false;
    }

    // ------------------------------------------------------------------------
    // 3. World Projection Texture 반복 Sampler
    // ------------------------------------------------------------------------
    if (!CreateSampler(
            device))
    {
        lastErrorMessage_ =
            L"Triplanar Sampler State 생성에 실패했습니다.";

        return false;
    }

    // ------------------------------------------------------------------------
    // 4. Material Constant Buffer
    // ------------------------------------------------------------------------
    if (!settingsBuffer_.Initialize(
            device))
    {
        lastErrorMessage_ =
            L"Triplanar Settings Constant Buffer 생성에 실패했습니다.";

        return false;
    }

    settings_.Settings0 =
    {
        desc.ProjectionScale,
        desc.BlendSharpness,
        desc.GrassFadeStartHeight,
        desc.GrassFadeEndHeight
    };

    settings_.Settings1 =
    {
        desc.RockSlopeStart,
        desc.RockSlopeEnd,
        desc.SnowStartHeight,
        desc.SnowFullHeight
    };

    return true;
}

void TriplanarMaterial::Bind(
    ID3D11DeviceContext* context)
{
    shader_.Bind(
        context);

    // t0 = Grass
    // t1 = Rock
    // t2 = Snow
    grassTexture_.BindPS(
        context,
        0u);

    rockTexture_.BindPS(
        context,
        1u);

    snowTexture_.BindPS(
        context,
        2u);

    ID3D11SamplerState* sampler =
        tiledSampler_.Get();

    context->PSSetSamplers(
        0u,
        1u,
        &sampler);

    settingsBuffer_.Update(
        context,
        settings_);

    // b0은 Terrain Transform Buffer가 사용하므로
    // Triplanar Material 설정은 b1을 사용한다.
    settingsBuffer_.BindPS(
        context,
        1u);
}

void TriplanarMaterial::Unbind(
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

    ID3D11SamplerState* nullSampler =
        nullptr;

    context->PSSetSamplers(
        0u,
        1u,
        &nullSampler);
}

const std::wstring& TriplanarMaterial::GetLastErrorMessage() const
{
    return lastErrorMessage_;
}

bool TriplanarMaterial::CreateSampler(
    ID3D11Device* device)
{
    D3D11_SAMPLER_DESC samplerDesc = {};

    samplerDesc.Filter =
        D3D11_FILTER_MIN_MAG_MIP_LINEAR;

    // World Position 기반 UV는 자연스럽게 0~1 범위를 넘으므로
    // WRAP 방식이 필요하다.
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
