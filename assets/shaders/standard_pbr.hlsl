#include "shaders/vertex/standard.hlsl"

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

    output.WorldPos = mul(World, input.Pos).xyz;
    output.Normal   = mul((float3x3)InverseTrans, input.Normal.xyz);
    output.Tangent  = float4(mul((float3x3)InverseTrans, input.Tangent.xyz), input.Tangent.w);
    output.Color    = input.Color;
    output.UV       = input.UV;

    output.Pos = mul(VIEW_INFO.WorldToClip, float4(output.WorldPos, 1.0));
    return output;
}

#include "shaders/layout/standard_shading.hlsl"

RWTexture2D<float4> tex;

float4 FSMain(VSOutput input) : SV_TARGET
{
    float4 t1 = AlbedoMap.Sample(AlbedoSampler, input.UV.xy);
    float4 t2 = NormalMap.Sample(NormalSampler, input.UV.xy);
    float4 t3 = AoMap.Sample(AoSampler, input.UV.xy);
    float4 t4 = MetallicRoughnessMap.Sample(MetallicRoughnessSampler, input.UV.xy);
    float4 t5 = EmissiveMap.Sample(EmissiveSampler, input.UV.xy);

    tex[int2(0, 0)] = float4(1, 1, 1, 1);

    return (t1 + t2 + t3 + t4 + t5) * Metallic * Roughness * Albedo;
}