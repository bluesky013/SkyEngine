#include "shaders/vertex/standard.hlsl"

#include "shaders/layout/default_pass.hlsl"
#include "shaders/layout/default_local.hlsl"

#if VIEW_COUNT > 1
#define VIEW_INFO View[ViewIndex]
#else
#define VIEW_INFO View
#endif

VSOutput VSMain(VSInput input
#if VIEW_COUNT > 1
    , uint ViewIndex : SV_ViewID
#endif
)
{
    VSOutput output = (VSOutput)0;

    float4x4 worldMatrix = World;
#if ENABLE_SKIN
	float4x4 skinMat =
		mul(Bones[input.joints.x], input.weights.x) +
		mul(Bones[input.joints.y], input.weights.y) +
		mul(Bones[input.joints.z], input.weights.z) +
		mul(Bones[input.joints.w], input.weights.w);
	worldMatrix = skinMat;
#endif

    output.WorldPos = mul(worldMatrix, input.Pos).xyz;
    output.Normal   = mul((float3x3)worldMatrix, input.Normal.xyz);
    output.Tangent  = float4(mul((float3x3)worldMatrix, input.Tangent.xyz), input.Tangent.w);
    output.Color    = input.Color;
    output.UV       = input.UV;

    output.Pos = mul(VIEW_INFO.ViewProj, float4(output.WorldPos, 1.0));
    return output;
}

#include "shaders/layout/standard_shading.hlsl"
#include "shaders/lighting/pbr.hlsl"

// static const float4x4 biasMat = float4x4(
// 	0.5, 0.0, 0.0, 0.5,
// 	0.0, 0.5, 0.0, 0.5,
// 	0.0, 0.0, 1.0, 0.0,
// 	0.0, 0.0, 0.0, 1.0 );
//
// float textureProj(float4 shadowCoord, float2 off)
// {
// 	float shadow = 1.0;
// 	float bias = 0.001;
// 	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
// 	{
// 		float dist = ShadowMap.Sample( ShadowMapSampler, shadowCoord.xy + off ).r;
// 		if ( shadowCoord.w > 0.0 && dist + bias < shadowCoord.z )
// 		{
// 			shadow = 0.05;
// 		}
// 	}
// 	return shadow;
// }
//
// float filterPCF(float4 sc)
// {
// 	int2 texDim;
// 	ShadowMap.GetDimensions(texDim.x, texDim.y);
// 	float scale = 1.5;
// 	float dx = scale * 1.0 / float(texDim.x);
// 	float dy = scale * 1.0 / float(texDim.y);
//
// 	float shadowFactor = 0.0;
// 	int count = 0;
// 	int range = 3;
//
// 	for (int x = -range; x <= range; x++)
// 	{
// 		for (int y = -range; y <= range; y++)
// 		{
// 			shadowFactor += textureProj(sc, float2(dx*x, dy*y));
// 			count++;
// 		}
//
// 	}
// 	return shadowFactor / count;
// }

float4 FSMain(VSOutput input) : SV_TARGET
{
    LightInfo light;
    light.Color = float4(1.0, 1.0, 1.0, 1.0);
    light.Direction = float4(float3(-0.2, -1, -0.5), 100.0);

#if ENABLE_NORMAL_MAP
    float3 tNormal = NormalMap.Sample(NormalSampler, input.UV.xy).xyz * 2.0 - 1.0;
    float3 N = normalize(input.Normal);
    float3 T = normalize(input.Tangent.xyz);
    float3 B = normalize(cross(N, input.Tangent.xyz)) * input.Tangent.w;
    float3x3 TBN = transpose(float3x3(T, B, N));
    N = normalize(mul(TBN, tNormal));
#else
    float3 N = normalize(input.Normal);
#endif

    float3 viewPos = float3(VIEW_INFO.World[0][3], VIEW_INFO.World[1][3], VIEW_INFO.World[2][3]);
    float3 L = normalize(-light.Direction.xyz);
    float3 V = normalize(viewPos - input.WorldPos);

    float4 texAlbedo = AlbedoMap.Sample(AlbedoSampler, input.UV.xy);
    float4 albedo = input.Color * Albedo * float4(pow(texAlbedo.rgb, 2.2), texAlbedo.a);

    StandardPBR pbrParam;
    pbrParam.Albedo    = albedo.xyz;
    pbrParam.Metallic  = Metallic;
    pbrParam.Roughness = Roughness;
    pbrParam.AO        = 1.0;
    pbrParam.Emissive  = float4(0, 0, 0, 0);

#if ENABLE_MR_MAP
    float4 mr = MetallicRoughnessMap.Sample(MetallicRoughnessSampler, input.UV.xy);
    pbrParam.Metallic = mr.b;
    pbrParam.Roughness = mr.g;
#endif

#if ENABLE_AO_MAP
    pbrParam.AO = AoMap.Sample(AoSampler, input.UV.xy).x;
#endif

#if ENABLE_EMISSIVE_MAP
    pbrParam.Emissive = EmissiveMap.Sample(EmissiveSampler, input.UV.xy);
#endif

    float shadow = 1.0;
//  float4 fragPosLightSpace = mul(biasMat, mul(LightMatrix, float4(input.WorldPos, 1.0)));
//  float4 shadowCoord = fragPosLightSpace / fragPosLightSpace.w;
// 	float shadow = filterPCF(shadowCoord);
// 	shadow = max(shadow, 1.0);
    float3 e0 = BRDF(V, N, light, pbrParam) * shadow;
    return float4(e0, albedo.a);
}