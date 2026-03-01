#pragma option({"key": "ENABLE_INSTANCE",     "default": 0, "type": "Batch"})

struct VSInput
{
    float3 Pos   : POSITION;

#if ENABLE_INSTANCE
    float3 Center : INST0;
    float3 Scale  : INST1;
    float4 Color  : INST2;
#endif
};

struct VSOutput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR;
};

#include "layout/default_global_view.hlslh"

#if VIEW_COUNT > 1
#define VIEW_INFO View[ViewIndex]
#else
#define VIEW_INFO View
#endif

VSOutput VSMain(VSInput input)
{
    VSOutput output = (VSOutput)0;

#if ENABLE_INSTANCE
    float3 pos = (input.Pos * input.Scale) + input.Center;
#else
    float3 pos = input.Pos;
#endif

    output.Pos   = mul(VIEW_INFO.ViewProj, float4(pos, 1.0));
    output.Color = input.Color;
    return output;
}

float4 FSMain(VSOutput input) : SV_TARGET
{
    return input.Color;
}