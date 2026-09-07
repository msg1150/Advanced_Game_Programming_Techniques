// ============================================================================
// TerrainVS.hlsl
// ----------------------------------------------------------------------------
// HeightMap Terrain 전용 기본 Vertex Shader.
//
// 현재 단계의 목표는 HeightMap 이미지가 지형 형태로 잘 변환되는지 확인하는 것이다.
// 따라서 Position을 WVP로 변환하고 Vertex Color를 그대로 Pixel Shader로 전달한다.
// ============================================================================

cbuffer CBTransform : register(b0)
{
    float4x4 WorldViewProjection;
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 UV       : TEXCOORD0;
    float4 Color    : COLOR0;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.Position =
        mul(
            float4(input.Position, 1.0f),
            WorldViewProjection);

    output.Color = input.Color;

    return output;
}
