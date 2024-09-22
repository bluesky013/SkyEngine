#include "shaders/vertex/position_only.hlsl"

#include "shaders/layout/default_pass.hlsl"
#include "shaders/layout/default_local.hlsl"

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
    return float4(245.0 / 255.0, 66.0 / 255.0, 206.0 / 255.0, 1.0);
}