// ============================================================================
// CollisionDebugVS.hlsl
// ----------------------------------------------------------------------------
// CPU에서 World Position으로 만든 Collision Wireframe Vertex를 View/Projection
// Matrix로 Clip Space에 변환한다.
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
    output.Position=mul(float4(input.Position,1.0f),WorldViewProjection);
    output.Color=input.Color;
    return output;
}
