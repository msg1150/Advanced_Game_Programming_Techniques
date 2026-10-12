// ============================================================================
// TriplanarGeomorphDebugPS.hlsl
// ----------------------------------------------------------------------------
// QuadTree LOD Geomorphing의 전이 진행률만 확인하기 위한 선택적 Debug PS.
// 기존 TriplanarPS.hlsl은 전혀 수정하지 않는다.
//
// 색상 의미:
//   Red    : 0%   - Parent LOD 표면에서 전이를 막 시작한 상태
//   Yellow : 50%  - Parent -> Child 전이 중간
//   Green  : 100% - Child LOD의 원래 표면에 도달한 상태
//
// 이 Shader는 렌더링 확인용일 뿐 LOD 선택, Morph Factor 계산,
// Streaming/Prefetch/Cache에는 아무 영향도 주지 않는다.
// 파일을 제거하거나 Debug 토글을 끄면 일반 Triplanar Geomorph 렌더링을 사용한다.
// ============================================================================

struct PSInput
{
    float4 Position      : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
    float3 WorldNormal   : TEXCOORD1;
    float  MorphProgress : TEXCOORD2;
};

float4 PSMain(
    PSInput input) : SV_TARGET
{
    const float progress = saturate(input.MorphProgress);

    // 0.0 -> 0.5 : Red -> Yellow
    // 0.5 -> 1.0 : Yellow -> Green
    // 분기 없이 같은 Gradient를 만들 수 있다.
    const float3 debugColor =
        float3(
            saturate(2.0f - progress * 2.0f),
            saturate(progress * 2.0f),
            0.0f);

    return float4(debugColor, 1.0f);
}
