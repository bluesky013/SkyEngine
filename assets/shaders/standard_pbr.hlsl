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
#include "shaders/lighting/pbr.hlsl"

float4 FSMain(VSOutput input) : SV_TARGET
{
    LightInfo light;
    light.Color = float4(1.0, 1.0, 1.0, 1.0);
    light.Direction = float4(normalize(float3(-1, -1, -1)), 1.0);

    float3 tNormal = NormalMap.Sample(NormalSampler, input.UV.xy).xyz * 2.0 - 1.0;
    float3 N = normalize(input.Normal);
    float3 T = normalize(input.Tangent.xyz);
    float3 B = normalize(cross(N, input.Tangent.xyz)) * input.Tangent.w;
    float3x3 TBN = transpose(float3x3(T, B, N));

    N = normalize(mul(TBN, tNormal));

    float3 viewPos = float3(VIEW_INFO.ViewToWorld[0][3], VIEW_INFO.ViewToWorld[1][3], VIEW_INFO.ViewToWorld[2][3]);
    float3 L = -light.Direction.xyz;
    float3 V = normalize(viewPos - input.WorldPos);

    float4 texAlbedo = AlbedoMap.Sample(AlbedoSampler, input.UV.xy);
    float4 albedo = input.Color * Albedo * float4(pow(texAlbedo.rgb, 2.2), texAlbedo.a);

    StandardPBR pbrParam;
    pbrParam.Albedo = albedo.xyz;

    float4 mr = MetallicRoughnessMap.Sample(MetallicRoughnessSampler, input.UV.xy);
    pbrParam.Metallic = mr.b;
    pbrParam.Roughness = mr.g;

    float4 ao = AoMap.Sample(AoSampler, input.UV.xy);
    float4 emissive = EmissiveMap.Sample(EmissiveSampler, input.UV.xy);

    float3 e0 = BRDF(V, N, light, pbrParam) + emissive.xyz;
    return float4(e0, albedo.a) ;
}