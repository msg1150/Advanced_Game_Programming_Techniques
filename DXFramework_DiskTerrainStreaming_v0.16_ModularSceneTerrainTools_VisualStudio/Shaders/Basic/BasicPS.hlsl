// ============================================================================
// BasicPS.hlsl
// ----------------------------------------------------------------------------
// 프레임워크 검증용 기본 Pixel Shader.
//
// Vertex Shader에서 받은 Color를 그대로 RenderTarget에 출력한다.
// 이후 Texture / Lighting 테스트가 추가되면 별도 Shader로 확장한다.
// ============================================================================

struct PSInput
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR0;
};

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.Color;
}
