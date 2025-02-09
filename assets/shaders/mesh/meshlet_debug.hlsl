struct VSInput
{
    float4 Center : CUSTOM0;
    float4 Apex   : CUSTOM1;
    float4 Axis   : CUSTOM2;

    uint VertexID   : SV_VertexID;
    uint InstanceID : SV_InstanceID;
};

struct VSOutput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR;
};

#include "layout/default_pass.hlslh"
#include "layout/default_local.hlslh"
#include "common/constants.hlslh"
#include "common/hash.hlslh"

#if VIEW_COUNT > 1
#define VIEW_INFO View[ViewIndex]
#else
#define VIEW_INFO View
#endif

float3 CalculateLocalBasis(float3 axis)
{
    float3 u = float3(1.0, 0.0, 0.0);
    if (abs(dot(axis, u)) > 0.999)
    {
        u = float3(0.0, 1.0, 0.0);
    }
    float3 v = normalize(cross(axis, u));
    u = normalize(cross(v, axis));
    return u;
}

VSOutput VSMain(VSInput input)
{
    VSOutput output = (VSOutput)0;

    float3 axis = -mul((float3x3)World, input.Axis.xyz);
    float3 apex = mul((float3x3)World, input.Apex.xyz);

    float3 Position;
    if (input.VertexID == 0 || length(axis) < 0.001)
    {
        Position = apex.xyz;
    }
    else
    {
        float scale = 1.0;
        float3 u = CalculateLocalBasis(axis);
        float3 v = normalize(cross(axis, u));
        float angle = (2.0 * PI * input.VertexID) / 32;

        float radius = input.Axis.w < 0.5 ? scale : sqrt(1 - input.Axis.w * input.Axis.w) / input.Axis.w * scale;

        float3 center = apex + axis * scale;
        float3 circlePoint = center + radius * (cos(angle) * u + sin(angle) * v);
        Position = circlePoint;
    }
    output.Pos   = mul(VIEW_INFO.ViewProj, float4(Position, 1.0));
    output.Color = UnPackU32ToV4(MurmurHash(input.InstanceID));
    output.Color.a = 0.5;
    return output;
}

float4 FSMain(VSOutput input) : SV_TARGET
{
    return input.Color;
}