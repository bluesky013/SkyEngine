#include "shaders/vertex/full_screen.hlsl"

#include "shaders/color/color_common.hlsl"

[[vk::binding(0, 0)]] Texture2D InColor : register(t0, space0);
[[vk::binding(1, 0)]] SamplerState InColorSampler : register(s0, space0);

float4 FSMain(VSOutput input) : SV_TARGET
{
    float4 color = InColor.Sample(InColorSampler, input.UV);
    float3 ldr = ACES(color.xyz);
    return float4(pow(ldr, 1.0f / 2.2), color.w);
}