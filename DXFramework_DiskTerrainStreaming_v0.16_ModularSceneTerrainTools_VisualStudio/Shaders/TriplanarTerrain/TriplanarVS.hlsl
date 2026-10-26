// ============================================================================
// TriplanarVS.hlsl
// ----------------------------------------------------------------------------
// Triplanar Terrain 전용 Vertex Shader.
//
// Pixel Shader에서 World Position과 World Normal을 이용해
// X/Y/Z 방향 Texture Projection을 계산할 수 있도록 필요한 값을 전달한다.
//
// 기존 Mesh Vertex Format은 변경하지 않는다.
// ============================================================================

cbuffer CBTransform : register(b0)
{
    float4x4 WorldViewProjection;
    float4x4 World;
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
    float4 Position      : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
    float3 WorldNormal   : TEXCOORD1;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.Position =
        mul(
            float4(input.Position, 1.0f),
            WorldViewProjection);

    output.WorldPosition =
        mul(
            float4(input.Position, 1.0f),
            World).xyz;

    // 현재 Showcase Terrain은 Translation / Rotation / Uniform Scale을
    // 전제로 한다.
    //
    // Non-Uniform Scale까지 일반화해야 하는 단계가 오면
    // 별도 Normal Matrix(Inverse Transpose)를 추가할 수 있다.
    output.WorldNormal =
        mul(
            float4(input.Normal, 0.0f),
            World).xyz;

    return output;
}
