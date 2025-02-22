#include "vertex/full_screen.hlslh"

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD;
};

VSOutput VSMain(uint vid : SV_VertexID)
{
    VSOutput res;

    res.Pos = float4(FT_POSITIONS[vid], 0.0, 1.0);
    res.UV = FT_UVS[vid];
    return res;
}

#include "depth/depth.hlslh"
[[vk::binding(0, 0)]] cbuffer global : register(b0, space0) {
    float Near;
    float Far;
    float Padding[2];
}

[[vk::binding(1, 0)]] Texture2D InDepth : register(t0, space0);
[[vk::binding(2, 0)]] SamplerState InDepthSampler : register(s0, space0);

float4 FSMain(VSOutput input) : SV_TARGET
{
    float depthSample = InDepth.Sample(InDepthSampler, input.UV).r;
    return float4(DepthToLinear(depthSample, Near, Far), 0, 0, 0);
}