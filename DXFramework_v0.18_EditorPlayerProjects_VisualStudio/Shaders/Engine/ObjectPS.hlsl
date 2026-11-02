Texture2D Albedo:register(t0);SamplerState LinearSampler:register(s0);
cbuffer Material:register(b0){float3 Tint;uint Textured;};
struct Input {float4 Position:SV_POSITION;float2 UV:TEXCOORD0;float4 Color:COLOR0;};
float4 PSMain(Input input):SV_TARGET
{return input.Color*float4(Tint,1)*(Textured!=0?Albedo.Sample(LinearSampler,input.UV):float4(1,1,1,1));}
