struct VSInput
{
    float4 Pos   : POSITION;
    float4 Color : COLOR;
};

struct VSOutput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR;
};

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

    output.COLOR = input.Color;
    return output;
}

float4 FSMain(VSOutput input) : SV_TARGET
{
    return input.Color;
}