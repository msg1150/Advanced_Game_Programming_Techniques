// ============================================================================
// TerrainPS.hlsl
// ----------------------------------------------------------------------------
// Perlin Terrain 전용 기본 Pixel Shader.
//
// 현재 Wireframe 단계에서는 Vertex Color를 그대로 출력한다.
// 이후 Lighting / Height Color / Texture Blending은
// 이 Terrain 전용 Shader에 추가하면 된다.
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
