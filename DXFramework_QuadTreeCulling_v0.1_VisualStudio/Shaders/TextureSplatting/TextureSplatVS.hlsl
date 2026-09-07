// ============================================================================
// TextureSplatVS.hlsl
// ----------------------------------------------------------------------------
// 자동 Texture Splatting Terrain 전용 Vertex Shader.
//
// Pixel Shader가 Texture Weight를 계산할 수 있도록 다음 값을 전달한다.
// - World Position : Height 계산
// - World Normal   : Slope 계산
// - UV             : 반복 Terrain Texture Sampling
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
    float2 UV            : TEXCOORD2;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.Position =
        mul(
            float4(input.Position, 1.0f),
            WorldViewProjection);

    // World Height 계산에 사용할 위치.
    output.WorldPosition =
        mul(
            float4(input.Position, 1.0f),
            World).xyz;

    // w=0으로 곱해 Translation이 Normal에 영향을 주지 않게 한다.
    //
    // 현재 Terrain Showcase에서는 Rotation/Translation과 Uniform Scale 사용을
    // 전제로 하며, 향후 Non-Uniform Scale이 필요하면 별도 Normal Matrix로
    // 확장할 수 있다.
    output.WorldNormal =
        mul(
            float4(input.Normal, 0.0f),
            World).xyz;

    output.UV =
        input.UV;

    return output;
}
