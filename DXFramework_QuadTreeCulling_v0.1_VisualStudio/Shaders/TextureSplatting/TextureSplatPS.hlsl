// ============================================================================
// TextureSplatPS.hlsl
// ----------------------------------------------------------------------------
// Terrain의 높이(Height)와 경사도(Slope)를 이용해
// Grass / Rock / Snow Texture Weight를 자동 생성하는 Pixel Shader.
//
// 기본 규칙:
// - 낮고 완만한 지형 -> Grass
// - 경사가 큰 지형   -> Rock
// - 높고 완만한 지형 -> Snow
//
// 각 Weight는 smoothstep으로 계산해 Texture 사이의 경계가 끊기지 않고
// 부드럽게 Blend되도록 한다.
// ============================================================================

Texture2D GrassTexture : register(t0);
Texture2D RockTexture  : register(t1);
Texture2D SnowTexture  : register(t2);

SamplerState TiledSampler : register(s0);

cbuffer CBMaterialSettings : register(b1)
{
    float TextureTiling;
    float GrassFadeStartHeight;
    float GrassFadeEndHeight;
    float RockSlopeStart;

    float RockSlopeEnd;
    float SnowStartHeight;
    float SnowFullHeight;
    float MaterialPadding;
};

struct PSInput
{
    float4 Position      : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
    float3 WorldNormal   : TEXCOORD1;
    float2 UV            : TEXCOORD2;
};

float4 PSMain(PSInput input) : SV_TARGET
{
    // ------------------------------------------------------------------------
    // 1. 반복 Texture Sampling
    // ------------------------------------------------------------------------
    const float2 tiledUV =
        input.UV *
        TextureTiling;

    const float4 grassColor =
        GrassTexture.Sample(
            TiledSampler,
            tiledUV);

    const float4 rockColor =
        RockTexture.Sample(
            TiledSampler,
            tiledUV);

    const float4 snowColor =
        SnowTexture.Sample(
            TiledSampler,
            tiledUV);

    // ------------------------------------------------------------------------
    // 2. Height
    //
    // World Position Y를 그대로 사용한다.
    // ------------------------------------------------------------------------
    const float height =
        input.WorldPosition.y;

    // ------------------------------------------------------------------------
    // 3. Slope
    //
    // Up Vector와 Surface Normal의 내적으로 평평한 정도를 계산한다.
    //
    // 평지:
    // dot ≈ 1
    // slope ≈ 0
    //
    // 수직에 가까운 절벽:
    // dot ≈ 0
    // slope ≈ 1
    // ------------------------------------------------------------------------
    const float3 normal =
        normalize(
            input.WorldNormal);

    const float upDot =
        saturate(
            dot(
                normal,
                float3(
                    0.0f,
                    1.0f,
                    0.0f)));

    const float slope =
        1.0f -
        upDot;

    // ------------------------------------------------------------------------
    // 4. Rock Weight
    //
    // 경사가 커질수록 Rock 비중이 증가한다.
    // ------------------------------------------------------------------------
    const float rockBySlope =
        smoothstep(
            RockSlopeStart,
            RockSlopeEnd,
            slope);

    // ------------------------------------------------------------------------
    // 5. Snow Weight
    //
    // 높이가 올라갈수록 Snow가 증가한다.
    // 다만 가파른 절벽에서는 눈이 덜 쌓이는 형태를 만들기 위해
    // Rock Weight가 높은 곳에서는 Snow를 줄인다.
    // ------------------------------------------------------------------------
    const float snowByHeight =
        smoothstep(
            SnowStartHeight,
            SnowFullHeight,
            height);

    const float snowWeight =
        snowByHeight *
        (1.0f - rockBySlope);

    // ------------------------------------------------------------------------
    // 6. Grass Weight
    //
    // 낮은 높이 + 완만한 경사에서 Grass가 강하다.
    // 높이가 Grass 범위를 벗어나거나 경사가 커지면 감소한다.
    // ------------------------------------------------------------------------
    const float grassHeightMask =
        1.0f -
        smoothstep(
            GrassFadeStartHeight,
            GrassFadeEndHeight,
            height);

    const float grassWeight =
        grassHeightMask *
        (1.0f - rockBySlope) *
        (1.0f - snowWeight);

    // ------------------------------------------------------------------------
    // 7. Rock 최종 Weight
    //
    // 가파른 지역은 높이와 관계없이 Rock이 우선한다.
    //
    // 평평한 중간 높이에서 Grass와 Snow가 모두 거의 0이 되는 경우를 대비해
    // 남는 비중도 Rock에 넘겨서 Layer Weight 총합이 안정적으로 유지되게 한다.
    // ------------------------------------------------------------------------
    const float uncoveredWeight =
        saturate(
            1.0f -
            grassWeight -
            snowWeight);

    const float rockWeight =
        max(
            rockBySlope,
            uncoveredWeight);

    // ------------------------------------------------------------------------
    // 8. 최종 Weight 정규화
    // ------------------------------------------------------------------------
    float3 weights =
        max(
            float3(
                grassWeight,
                rockWeight,
                snowWeight),
            0.0f);

    const float weightSum =
        weights.r +
        weights.g +
        weights.b;

    if (weightSum > 0.0001f)
    {
        weights /=
            weightSum;
    }
    else
    {
        weights =
            float3(
                1.0f,
                0.0f,
                0.0f);
    }

    // ------------------------------------------------------------------------
    // 9. Texture Blend
    // ------------------------------------------------------------------------
    const float3 linearColor =
        grassColor.rgb * weights.r +
        rockColor.rgb  * weights.g +
        snowColor.rgb  * weights.b;

    // ------------------------------------------------------------------------
    // 현재 Framework BackBuffer가 UNORM이며 SRGB RenderTarget이 아니므로,
    // SRGB Texture Sampling 결과(Linear)를 화면 표시용으로 근사 Gamma Encode한다.
    //
    // 이후 Framework에 SRGB BackBuffer/RTV를 정식으로 추가하면
    // 이 보정은 Renderer 단계로 옮길 수 있다.
    // ------------------------------------------------------------------------
    const float3 displayColor =
        pow(
            saturate(linearColor),
            1.0f / 2.2f);

    return float4(
        displayColor,
        1.0f);
}
