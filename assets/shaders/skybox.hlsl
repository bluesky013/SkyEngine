struct VSInput
{
    float4 Pos : POSITION;
    float2 UV  : UV;
};

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float2 UV  : UV;
};

#include "layout/default_pass.hlslh"

#if VIEW_COUNT > 1
#define VIEW_INFO View[ViewIndex]
#else
#define VIEW_INFO View
#endif

VSOutput VSMain(VSInput input)
{
    VSOutput output = (VSOutput)0;

    output.Pos = mul(VIEW_INFO.ViewProj, float4(input.Pos.xyz, 1.0));
    output.UV  = input.UV;
    return output;
}

[[vk::binding(0, 1)]] Texture2D SkyBox : register(t0, space1);
[[vk::binding(1, 1)]] SamplerState SkyBoxSampler : register(s0, space1);

float4 FSMain(VSOutput input) : SV_TARGET
{
    float4 color = SkyBox.Sample(SkyBoxSampler, input.UV);
    return float4(pow(color.rgb, 2.2), 1.0);
}