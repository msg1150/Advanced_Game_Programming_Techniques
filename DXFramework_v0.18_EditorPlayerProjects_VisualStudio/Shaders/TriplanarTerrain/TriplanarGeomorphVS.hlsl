// ============================================================================
// TriplanarGeomorphVS.hlsl
// ----------------------------------------------------------------------------
// 기존 TriplanarVS의 출력 규약을 그대로 유지하면서 Position.y와 Normal만
// Parent LOD 표면으로 부드럽게 보간하는 Vertex Shader.
//
// Geomorph 전용 Mesh에서 Vertex Color는 다음 의미로 사용한다.
// Color.x   = Parent 표면의 목표 Y
// Color.yzw = Parent 표면의 목표 Normal
//
// 기존 TriplanarVS / Pixel Shader / Texture 로직은 수정하지 않는다.
// Geomorph Feature를 제거하려면 이 Shader와 Feature 폴더 및 Manager 연결부만
// 제거하면 기존 Shader가 그대로 남는다.
// ============================================================================

cbuffer CBTransform : register(b0)
{
    float4x4 WorldViewProjection;
    float4x4 World;
};

cbuffer CBGeomorph : register(b2)
{
    float MorphFactor;
    float3 GeomorphPadding;
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

    // Debug Pixel Shader에서 사용하는 전이 진행률.
    // 내부 MorphFactor는 1=Parent 모양, 0=Child 원본 모양이므로
    // 사람이 보기 쉬운 0%=전이 시작, 100%=전이 완료로 뒤집어 전달한다.
    float MorphProgress  : TEXCOORD2;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    const float morph = saturate(MorphFactor);

    float3 localPosition = input.Position;

    // Child가 처음 나타날 때(morph=1)는 Parent Patch의 동일 XZ 위치 높이와
    // 정확히 맞고, Camera가 가까워질수록(morph->0) 원본 높이로 복귀한다.
    localPosition.y =
        lerp(
            input.Position.y,
            input.Color.x,
            morph);

    const float3 targetNormal = input.Color.yzw;

    float3 localNormal =
        normalize(
            lerp(
                input.Normal,
                targetNormal,
                morph));

    output.Position =
        mul(
            float4(localPosition, 1.0f),
            WorldViewProjection);

    output.WorldPosition =
        mul(
            float4(localPosition, 1.0f),
            World).xyz;

    output.WorldNormal =
        mul(
            float4(localNormal, 0.0f),
            World).xyz;

    // Debug 표시 기준:
    //   0.0 = Parent 표면에서 전이를 시작한 상태
    //   0.5 = 전이 중간
    //   1.0 = Child 원본 표면으로 전이가 끝난 상태
    output.MorphProgress = 1.0f - morph;

    return output;
}
