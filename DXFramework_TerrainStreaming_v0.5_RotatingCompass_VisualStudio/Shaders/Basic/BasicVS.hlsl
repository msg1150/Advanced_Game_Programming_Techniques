// ============================================================================
// BasicVS.hlsl
// ----------------------------------------------------------------------------
// 프레임워크 검증용 기본 Vertex Shader.
//
// 현재 역할:
// - Object Local Position을 WVP Matrix로 Clip Space에 변환
// - Vertex Color를 Pixel Shader로 전달
//
// Normal / UV는 아직 출력에 사용하지 않지만
// Vertex Layout 확장성을 유지하기 위해 Input에는 포함한다.
// ============================================================================

// C++ Application::CBTransform이 b0 Slot에 전달된다.
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

    // Local Position -> Clip Space.
    output.Position =
        mul(
            float4(input.Position, 1.0f),
            WorldViewProjection);

    // 현재는 Lighting 계산 없이 Vertex Color를 그대로 전달한다.
    output.Color =
        input.Color;

    return output;
}
