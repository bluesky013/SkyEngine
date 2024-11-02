struct VSInput
{
    float3 Pos   : POSITION;
    float4 Color : COLOR;
};

struct VSOutput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR;
};

#include "shaders/layout/default_pass.hlsl"

#if VIEW_COUNT > 1
#define VIEW_INFO View[ViewIndex]
#else
#define VIEW_INFO View
#endif

VSOutput VSMain(VSInput input)
{
    VSOutput output = (VSOutput)0;

    output.Pos   = mul(VIEW_INFO.ViewProj, float4(input.Pos.xyz, 1.0));
    output.Color = input.Color;
    return output;
}

float4 FSMain(VSOutput input) : SV_TARGET
{
    return input.Color * 10.0;
}