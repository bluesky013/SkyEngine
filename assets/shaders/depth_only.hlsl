#include "vertex/position_only.hlslh"

#include "layout/default_pass.hlslh"
#include "layout/default_local.hlslh"

#if VIEW_COUNT > 1
#define VIEW_INFO View[ViewIndex]
#else
#define VIEW_INFO View
#endif

VSOutput VSMain(VSInput input)
{
    VSOutput output = (VSOutput)0;

    float4 WorldPos = mul(World, input.Pos);
    output.Pos = mul(VIEW_INFO.ViewProj, float4(WorldPos.xyz, 1.0));
    return output;
}

float4 FSMain() : SV_TARGET
{
    return float4(0.0, 0.0, 0.0, 0.0);
}