#include "shaders/vertex/full_screen.hlsl"

struct VSOutput
{
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float2 UV  : TEXCOORD;
};

VSOutput VSMain(uint vid : SV_VertexID)
{
    VSOutput res;

    res.Pos = float4(FT_POSITIONS[vid], 0.0, 1.0);
    res.UV = FT_UVS[vid];
    return res;
}

#include "shaders/color/color_common.hlsl"
[[vk::binding(0, 0)]] Texture2D InColor : register(t0, space0);
[[vk::binding(1, 0)]] SamplerState InColorSampler : register(s0, space0);

float4 FSMain(VSOutput input) : SV_TARGET
{
    float4 color = InColor.Sample(InColorSampler, input.UV);
    float3 ldr = ACES(color.xyz);
    return float4(pow(ldr, 1.0f / 2.2), color.w);
}