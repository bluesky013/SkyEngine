#pragma option({"key": "ENABLE_SKIN",         "default": 0, "type": "Batch"})
#pragma option({"key": "MESH_SHADER",         "default": 0, "type": "Batch"})
#pragma option({"key": "MESH_SHADER_DEBUG",   "default": 0, "type": "Batch"})

#pragma option({"key": "ENABLE_INSTANCE",     "default": 0, "type": "Batch"})
#pragma option({"key": "ENABLE_NORMAL_MAP",   "default": 0, "type": "Batch"})
#pragma option({"key": "ENABLE_EMISSIVE_MAP", "default": 0, "type": "Batch"})
#pragma option({"key": "ENABLE_AO_MAP",       "default": 0, "type": "Batch"})
#pragma option({"key": "ENABLE_MR_MAP",       "default": 0, "type": "Batch"})
#pragma option({"key": "ENABLE_ALPHA_MASK",   "default": 0, "type": "Batch"})
#pragma option({"key": "ENABLE_IBL",          "default": 0, "type": "Batch"})

#pragma option({"key": "ENABLE_SHADOW",       "default": 0, "type": "Pass"})

#include "vertex/standard.hlslh"

#include "layout/default_pass.hlslh"
#include "layout/default_local.hlslh"

#if VIEW_COUNT > 1
#define VIEW_INFO View[ViewIndex]
#else
#define VIEW_INFO View
#endif

//----------------------------------------- Vertex Shader-----------------------------------------//
VSOutput VSMain(VSInput input)
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

    float4 pos = input.Pos;
#if ENABLE_INSTANCE
    pos += input.Offset;
#endif

    output.WorldPos = mul(worldMatrix, pos).xyz;
    output.Normal   = mul((float3x3)worldMatrix, input.Normal.xyz);
    output.Tangent  = input.Tangent;
    output.Color    = input.Color;
    output.UV       = input.UV;

    output.Pos = mul(VIEW_INFO.ViewProj, float4(output.WorldPos, 1.0));
    return output;
}
//----------------------------------------- Vertex Shader-----------------------------------------//

// some compiler may not support parse mesh shader
#if MESH_SHADER

#include "common/hash.hlslh"

groupshared Payload TaskPayload;
//------------------------------------------ Task Shader------------------------------------------//
bool IsVisible(Meshlet m, float4x4 world, float3 viewPos)
{
//     float4 center = mul(world, float4(m.center.xyz, 1));
//     float radius = m.center.w;
    if (m.coneAxis.w >= 0.99) {
        return true;
    }

    float3 axis = normalize(mul((float3x3)World, m.coneAxis.xyz));
    float3 apex = mul((float3x3)World, m.coneApex.xyz);
    float3 view = normalize(viewPos - apex);

    if (dot(view, -axis) >= m.coneAxis.w)
    {
        return false;
    }
    return true;
}

[outputtopology("triangle")]
[numthreads(MESH_GROUP_SIZE,1,1)]
void TASMain(uint gtid : SV_GroupThreadID, uint dtid : SV_DispatchThreadID, uint gid : SV_GroupID)
{
    float3 viewPos = float3(VIEW_INFO.World[0][3], VIEW_INFO.World[1][3], VIEW_INFO.World[2][3]);

    bool visible = false;
    if (dtid < MeshletCount)
    {
        visible = IsVisible(Meshlets[dtid + FirstMeshlet], World, viewPos);
//         visible = Meshlets[dtid + FirstMeshlet].vertexCount > 0;
    }

    if (visible)
    {
        uint index = WavePrefixCountBits(visible);
        TaskPayload.MeshletIndices[index] = dtid;
    }
    uint visibleCount = WaveActiveCountBits(visible);
    DispatchMesh(visibleCount, 1, 1, TaskPayload);
}
//------------------------------------------ Task Shader------------------------------------------//

//------------------------------------------ Mesh Shader------------------------------------------//
uint GetVertexIndex(Meshlet meshlet, uint localIndex)
{
    return VertexIndices[meshlet.vertexOffset + localIndex];
}

uint3 UnpackPrimitive(uint primitive)
{
    return uint3(primitive & 0xFF, (primitive >> 8) & 0xFF, (primitive >> 16) & 0xFF);
}

uint3 GetPrimitive(Meshlet meshlet, uint index)
{
    return UnpackPrimitive(MeshletTriangles[meshlet.triangleOffset + index]);
}

VSOutput GetVertexAttributes(uint meshletIndex, uint vertexIndex)
{
    float4 pv = PositionBuf[vertexIndex];
    ExtVertex ev = ExtBuf[vertexIndex];

    VSOutput output = (VSOutput)0;
    output.WorldPos = mul(World, pv).xyz;
    output.Normal   = mul((float3x3)World, ev.normal.xyz);
    output.Tangent  = ev.tangent;
    output.Color    = ev.color;
    output.UV       = ev.uv;
    output.Pos      = mul(VIEW_INFO.ViewProj, float4(output.WorldPos, 1.0));

    output.MeshletIndex = meshletIndex;
    return output;
}

[outputtopology("triangle")]
[numthreads(128, 1, 1)]
void MSMain(
    uint dtid : SV_DispatchThreadID,
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    in payload Payload taskPayload,
    out vertices VSOutput vertices[MAX_VERTICES],
    out indices uint3 tris[MAX_PRIMS])
{
    uint meshletIndex = taskPayload.MeshletIndices[gid];
//     uint meshletIndex = gid;
    if (meshletIndex >= MeshletCount) {
        return;
    }

    meshletIndex += FirstMeshlet;

    Meshlet meshlet = Meshlets[meshletIndex];
    SetMeshOutputCounts(meshlet.vertexCount, meshlet.triangleCount);

    if (gtid < meshlet.vertexCount)
    {
        uint vertexIndex = GetVertexIndex(meshlet, gtid);
        vertices[gtid] = GetVertexAttributes(meshletIndex, vertexIndex);
    }

    if (gtid < meshlet.triangleCount)
    {
        tris[gtid] = GetPrimitive(meshlet, gtid);
    }

}
//------------------------------------------ Mesh Shader------------------------------------------//
#endif

#include "layout/standard_shading.hlslh"
#include "lighting/pbr.hlslh"

static const float4x4 biasMat = float4x4(
	0.5, 0.0, 0.0, 0.5,
	0.0, 0.5, 0.0, 0.5,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0 );

float TextureProj(float4 shadowCoord, float2 off)
{
	float shadow = 1.0;
	float bias = 0.01;
	if (shadowCoord.z > -1.0 && shadowCoord.z < 1.0) {
		float dist = ShadowMap.Sample(ShadowMapSampler, shadowCoord.xy + off).r;
		if (shadowCoord.w > 0.0 && dist + bias < shadowCoord.z) {
			shadow = 0.1;
		}
	}
	return shadow;
}

float FilterPCF(float4 sc)
{
	int2 texDim;
	ShadowMap.GetDimensions(texDim.x, texDim.y);
	float scale = 1.0;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;

	for (int x = -range; x <= range; x++) {
		for (int y = -range; y <= range; y++) {
			shadowFactor += TextureProj(sc, float2(dx*x, dy*y));
			count++;
		}
	}
	return shadowFactor / count;
}

float3 PrefilteredReflection(float3 R, float roughness)
{
    const float MAX_REFLECTION_LOD = 9.0; // 512 x 512
    float lod = roughness * MAX_REFLECTION_LOD;
    float lodf = floor(lod);
    float lodc = ceil(lod);
    float3 a = PrefilteredMap.SampleLevel(PrefilteredMapSampler, R, lodf).rgb;
    float3 b = PrefilteredMap.SampleLevel(PrefilteredMapSampler, R, lodc).rgb;
    return lerp(a, b, lod - lodf);
}

//------------------------------------------ Fragment Shader------------------------------------------//
float4 FSMain(VSOutput input) : SV_TARGET
{
    float4 texAlbedo = AlbedoMap.Sample(AlbedoSampler, input.UV.xy);
    float4 albedo = input.Color * Albedo * float4(pow(texAlbedo.rgb, 2.2), texAlbedo.a);

#if ENABLE_ALPHA_MASK
    if (albedo.w < AlphaCutoff) {
        discard;
    }
#endif

    LightInfo light;
    light.Color = MainLightColor;
    light.Direction = MainLightDirection;

#if ENABLE_NORMAL_MAP
    float3 tNormal = NormalMap.Sample(NormalSampler, input.UV.xy).xyz * 2.0 - float3(1.0, 1.0, 1.0);
    float3 N = normalize(input.Normal);
    float3 T = normalize(input.Tangent.xyz);
    float3 B = cross(N, input.Tangent.xyz) * input.Tangent.w;
    float3x3 TBN = transpose(float3x3(T, B, N));
    N = mul(TBN, normalize(tNormal));
#else
    float3 N = normalize(input.Normal);
#endif

    float3 viewPos = float3(VIEW_INFO.World[0][3], VIEW_INFO.World[1][3], VIEW_INFO.World[2][3]);
    float3 L = normalize(-light.Direction.xyz);
    float3 V = normalize(viewPos - input.WorldPos);

#if MESH_SHADER && MESH_SHADER_DEBUG
    return UnPackU32ToV4(MurmurHash(input.MeshletIndex));
#endif

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

    float4 fragPosLightSpace = mul(biasMat, mul(LightMatrix, float4(input.WorldPos, 1.0)));
    float4 shadowCoord = fragPosLightSpace / fragPosLightSpace.w;
	float shadow = FilterPCF(shadowCoord);

    float3 e0 = BRDF(V, N, light, pbrParam) * shadow;


    float3 R = reflect(-V, N);
    float2 brdf = BRDFLut.Sample(BRDFLutSampler, float2(max(dot(N, V), 0.0), pbrParam.Roughness)).rg;
    float3 reflection = PrefilteredReflection(R, pbrParam.Roughness).rgb;
    float3 irradiance = IrradianceMap.Sample(IrradianceSampler, N).rgb;

    float3 F0 = lerp(0.04, pbrParam.Albedo, pbrParam.Metallic);

	float3 diffuse = irradiance * pbrParam.Albedo;
	float3 F = F_SchlickR(max(dot(N, V), 0.0), F0, pbrParam.Roughness);
	float3 specular = reflection * (F * brdf.x + brdf.y);

	float3 kD = 1.0 - F;
    kD *= 1.0 - pbrParam.Metallic;
    float3 ambient = (kD * diffuse + specular);

    return float4(e0 + ambient * 0.05, albedo.a);
}
//------------------------------------------ Fragment Shader------------------------------------------//