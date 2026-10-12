// ============================================================================
// TriplanarPS.hlsl
// ----------------------------------------------------------------------------
// Terrain Texture를 X / Y / Z 세 방향에서 World Space Projection하고,
// Surface Normal이 향하는 방향을 Weight로 사용해 세 Projection을 Blend한다.
//
// 이 방식의 목적:
// - 평면 UV에 의한 절벽 Texture Stretching 감소
// - 가파른 Rock 영역에서도 Texture 밀도 유지
//
// Layer 선택 자체는 이전 Height / Slope Texture Splatting 규칙을 유지한다.
// ============================================================================

Texture2D GrassTexture : register(t0);
Texture2D RockTexture  : register(t1);
Texture2D SnowTexture  : register(t2);

SamplerState TiledSampler : register(s0);

cbuffer CBTriplanarSettings : register(b1)
{
    float ProjectionScale;
    float BlendSharpness;
    float GrassFadeStartHeight;
    float GrassFadeEndHeight;

    float RockSlopeStart;
    float RockSlopeEnd;
    float SnowStartHeight;
    float SnowFullHeight;
};

struct PSInput
{
    float4 Position      : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
    float3 WorldNormal   : TEXCOORD1;
};

// ----------------------------------------------------------------------------
// Surface Normal의 절대값을 이용해 X/Y/Z Projection Blend Weight를 만든다.
//
// 예:
// 위를 바라보는 평지
// -> Y Weight가 큼
//
// X 방향을 바라보는 절벽
// -> X Weight가 큼
// ----------------------------------------------------------------------------
float3 CalculateTriplanarWeights(
    float3 normal)
{
    float3 weights =
        pow(
            abs(normal),
            BlendSharpness);

    const float weightSum =
        max(
            weights.x +
            weights.y +
            weights.z,
            0.0001f);

    return weights /
           weightSum;
}

// ----------------------------------------------------------------------------
// 하나의 Texture를 세 축 방향에서 Sample하고 Blend한다.
// ----------------------------------------------------------------------------
float4 SampleTriplanar(
    Texture2D sourceTexture,
    float3 worldPosition,
    float3 normal,
    float3 projectionWeights)
{
    // X축 투영:
    // X 방향 Surface를 YZ 평면으로 투영.
    float2 uvX =
        worldPosition.zy *
        ProjectionScale;

    // Y축 투영:
    // 위/아래 Surface를 XZ 평면으로 투영.
    float2 uvY =
        worldPosition.xz *
        ProjectionScale;

    // Z축 투영:
    // Z 방향 Surface를 XY 평면으로 투영.
    float2 uvZ =
        worldPosition.xy *
        ProjectionScale;

    // 반대 방향 Face에서 Texture가 갑자기 좌우 반전되는 느낌을 줄이기 위해
    // 각 Projection의 한 축을 Normal 방향에 따라 뒤집는다.
    uvX.x *=
        normal.x < 0.0f
        ? -1.0f
        : 1.0f;

    uvY.x *=
        normal.y < 0.0f
        ? -1.0f
        : 1.0f;

    uvZ.x *=
        normal.z < 0.0f
        ? -1.0f
        : 1.0f;

    const float4 sampleX =
        sourceTexture.Sample(
            TiledSampler,
            uvX);

    const float4 sampleY =
        sourceTexture.Sample(
            TiledSampler,
            uvY);

    const float4 sampleZ =
        sourceTexture.Sample(
            TiledSampler,
            uvZ);

    return
        sampleX * projectionWeights.x +
        sampleY * projectionWeights.y +
        sampleZ * projectionWeights.z;
}

float4 PSMain(
    PSInput input) : SV_TARGET
{
    const float3 normal =
        normalize(
            input.WorldNormal);

    // ------------------------------------------------------------------------
    // 1. Triplanar Projection Weight
    // ------------------------------------------------------------------------
    const float3 projectionWeights =
        CalculateTriplanarWeights(
            normal);

    // ------------------------------------------------------------------------
    // 2. Grass / Rock / Snow를 각각 Triplanar Sample
    //
    // 기존 UV는 사용하지 않는다.
    // ------------------------------------------------------------------------
    const float4 grassColor =
        SampleTriplanar(
            GrassTexture,
            input.WorldPosition,
            normal,
            projectionWeights);

    const float4 rockColor =
        SampleTriplanar(
            RockTexture,
            input.WorldPosition,
            normal,
            projectionWeights);

    const float4 snowColor =
        SampleTriplanar(
            SnowTexture,
            input.WorldPosition,
            normal,
            projectionWeights);

    // ------------------------------------------------------------------------
    // 3. 이전 Texture Splatting과 동일한 Height / Slope Layer Weight
    // ------------------------------------------------------------------------
    const float height =
        input.WorldPosition.y;

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

    const float rockBySlope =
        smoothstep(
            RockSlopeStart,
            RockSlopeEnd,
            slope);

    const float snowByHeight =
        smoothstep(
            SnowStartHeight,
            SnowFullHeight,
            height);

    const float snowWeight =
        snowByHeight *
        (1.0f - rockBySlope);

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

    const float uncoveredWeight =
        saturate(
            1.0f -
            grassWeight -
            snowWeight);

    const float rockWeight =
        max(
            rockBySlope,
            uncoveredWeight);

    float3 layerWeights =
        max(
            float3(
                grassWeight,
                rockWeight,
                snowWeight),
            0.0f);

    const float layerWeightSum =
        layerWeights.r +
        layerWeights.g +
        layerWeights.b;

    if (layerWeightSum > 0.0001f)
    {
        layerWeights /=
            layerWeightSum;
    }
    else
    {
        layerWeights =
            float3(
                1.0f,
                0.0f,
                0.0f);
    }

    // ------------------------------------------------------------------------
    // 4. Layer Blend
    // ------------------------------------------------------------------------
    const float3 linearColor =
        grassColor.rgb * layerWeights.r +
        rockColor.rgb  * layerWeights.g +
        snowColor.rgb  * layerWeights.b;

    // 현재 Framework BackBuffer가 UNORM이므로
    // 이전 Texture Splatting 단계와 동일한 화면용 Gamma Encode를 유지한다.
    const float3 displayColor =
        pow(
            saturate(linearColor),
            1.0f / 2.2f);

    return float4(
        displayColor,
        1.0f);
}
