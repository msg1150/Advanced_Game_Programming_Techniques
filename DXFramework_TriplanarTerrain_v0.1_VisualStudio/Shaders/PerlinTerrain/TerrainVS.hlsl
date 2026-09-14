// ============================================================================
// TerrainVS.hlsl
// ----------------------------------------------------------------------------
// Perlin Terrain 전용 기본 Vertex Shader.
//
// 현재 단계에서는 Terrain의 형태와 Wireframe Grid를 확인하는 것이 목적이므로
// Position을 WVP Matrix로 변환하고 Vertex Color만 Pixel Shader로 전달한다.
//
// Normal / UV는 이미 Mesh에 포함되어 있으며
// 이후 Terrain Lighting / Texture 단계에서 그대로 사용할 예정이다.
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

    // Terrain Local Position을 Clip Space까지 변환한다.
    output.Position =
        mul(
            float4(input.Position, 1.0f),
            WorldViewProjection);

    // 현재는 별도 Lighting 없이 Generator가 만든 색상을 그대로 전달한다.
    output.Color =
        input.Color;

    return output;
}
