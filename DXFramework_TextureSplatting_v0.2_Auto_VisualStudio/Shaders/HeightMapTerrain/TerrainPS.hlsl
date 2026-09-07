// ============================================================================
// TerrainPS.hlsl
// ----------------------------------------------------------------------------
// HeightMap Terrain 전용 기본 Pixel Shader.
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
