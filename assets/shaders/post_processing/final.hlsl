#include "shaders/vertex/full_screen.hlsl"

#include "shaders/color/color_common.hlsl"

#if VIEW_COUNT > 1
[[vk::binding(0, 0)]] Texture2DArray InColor : register(t0, space0);
#else
[[vk::binding(0, 0)]] Texture2D InColor : register(t0, space0);
#endif

[[vk::binding(1, 0)]] SamplerState InColorSampler : register(s0, space0);

float4 FSMain(VSOutput input) : SV_TARGET
{
#if VIEW_COUNT > 1
    float4 color = InColor.Sample(InColorSampler, float3(input.UV, input.ViewIndex));
#else
    float4 color = InColor.Sample(InColorSampler, input.UV);
#endif
    float3 ldr = ACES(color.xyz);
    return float4(pow(ldr, 1.0f / 2.2), color.w);
}