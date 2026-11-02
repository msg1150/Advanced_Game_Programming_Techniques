cbuffer Transform : register(b0) { float4x4 WVP; };
struct Input { float3 Position:POSITION; float3 Normal:NORMAL; float2 UV:TEXCOORD0; float4 Color:COLOR0; };
struct Output { float4 Position:SV_POSITION; float2 UV:TEXCOORD0; float4 Color:COLOR0; };
Output VSMain(Input input)
{ Output o;o.Position=mul(float4(input.Position,1),WVP);o.UV=input.UV;o.Color=input.Color;return o; }
