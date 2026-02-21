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
#include "layout/default_skin.hlslh"

#if VIEW_COUNT > 1
#define VIEW_INFO View[ViewIndex]
#else
#define VIEW_INFO View
#endif

//----------------------------------------- Vertex Shader-----------------------------------------//
VSOutput VSMain(VSInput input)
{
    VSOutput output = (VSOutput)0;

#if ENABLE_SKIN
    float4x4 skinMat =
    	mul(Bones[input.joints.x], input.weights.x) +
    	mul(Bones[input.joints.y], input.weights.y) +
    	mul(Bones[input.joints.z], input.weights.z) +
    	mul(Bones[input.joints.w], input.weights.w);
    float4x4 worldMatrix = mul(World, 0) + skinMat;
#else
    float4x4 worldMatrix = World;
#endif

    float4 pos = input.Pos;
#if ENABLE_INSTANCE
    pos += input.Offset;
#endif

    output.WorldPos = mul(worldMatrix, pos).xyz;
    output.Normal   = mul((float3x3)worldMatrix, input.Normal.xyz);
    output.Tangent  = input.Tangent;
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
bool ConeVisible(Meshlet m, float4x4 world, float3 viewPos, float3 offset)
{
    if (m.coneAxis.w >= 0.99) {
        return true;
    }

    float3 axis = normalize(mul((float3x3)world, m.coneAxis.xyz));
    float3 apex = mul((float3x3)world, m.coneApex.xyz + offset);
    float3 view = normalize(viewPos - apex);

    if (dot(view, -axis) >= m.coneAxis.w)
    {
        return false;
    }
    return true;
}

float CalculateMip(float ddx, float ddy)
{
    return 0.5 * log2(max(ddx, ddy));
}

bool HZBSphereTest(float3 center, float radius, int startMip)
{
    float4 worldPos = mul(World, float4(center, 1.0));
    float4 clipPos = mul(VIEW_INFO.ViewProj, worldPos);
    clipPos.xyz /= clipPos.w;

    if (clipPos.w <= 0) return false;

    float2 screenUV = clamp(clipPos.xy * 0.5 + 0.5, 0.001, 0.999);
    float approxScreenRadius = radius / abs(clipPos.z);
    float minSphereDepth = clipPos.z - approxScreenRadius * 0.005;

    float2 texelSize = 1.0 / float2(Viewport.zw);

    float lod = CalculateMip(approxScreenRadius * texelSize, approxScreenRadius * texelSize);

    float2 sampleUVs[4] = {
        screenUV + float2(-approxScreenRadius, -approxScreenRadius) * texelSize,
        screenUV + float2( approxScreenRadius,  approxScreenRadius) * texelSize,
        screenUV + float2(-approxScreenRadius,  approxScreenRadius) * texelSize,
        screenUV + float2( approxScreenRadius, -approxScreenRadius) * texelSize
    };

    float maxHZB = 0;
    [unroll]
    for(int j = 0; j < 4; ++j)
    {
        float2 uv = clamp(sampleUVs[j], 0.0, 1.0);
        maxHZB = max(maxHZB, HizBuffer.SampleLevel(HizBufferSampler, uv, lod).r);
    }

    if(minSphereDepth > maxHZB)
    {
        return false;
    }

    /*
    int currentMip = min(startMip, maxDims - 1);

    const int MAX_ITERATION = 4;
    for (int i = 0; i < MAX_ITERATION; ++i)
    {
        uint2 mipSize;
        HizBuffer.GetDimensions(currentMip, mipSize.x, mipSize.y, maxDims);
        float2 texelSize = 1.0 / float2(mipSize);

        float2 sampleUVs[4] = {
            screenUV + float2(-approxScreenRadius, -approxScreenRadius) * texelSize,
            screenUV + float2( approxScreenRadius,  approxScreenRadius) * texelSize,
            screenUV + float2(-approxScreenRadius,  approxScreenRadius) * texelSize,
            screenUV + float2( approxScreenRadius, -approxScreenRadius) * texelSize
        };

        float maxHZB = 0;
        [unroll]
        for(int j = 0; j < 4; ++j)
        {
            float2 uv = clamp(sampleUVs[j], 0.0, 1.0);
            maxHZB = max(maxHZB, HizBuffer.SampleLevel(HizBufferSampler, uv, currentMip).r);
        }

        if(minSphereDepth > maxHZB)
        {
            return false;
        }

        if(currentMip == 0) break;
        currentMip = max(currentMip - 1, 0);

        if(approxScreenRadius * mipSize.x < 4.0) break;
    }*/
    return true;
}

[outputtopology("triangle")]
[numthreads(MESH_GROUP_SIZE,1,1)]
void TASMain(uint gtid : SV_GroupThreadID, uint2 dtid : SV_DispatchThreadID, uint gid : SV_GroupID)
{
    float3 viewPos = float3(VIEW_INFO.World[0][3], VIEW_INFO.World[1][3], VIEW_INFO.World[2][3]);

#if ENABLE_INSTANCE
    float4 offset = InstanceBuffer[dtid.y];
#else
    float4 offset = InstanceBuffer[0];
#endif

    bool visible = false;
    if (dtid.x < MeshletCount)
    {
        Meshlet meshlet = Meshlets[dtid.x + FirstMeshlet];
        visible = meshlet.vertexCount > 0;

//         if (visible) {
//             visible = ConeVisible(meshlet, World, viewPos, offset.xyz);
//         }

        if (visible) {
            visible = HZBSphereTest(meshlet.center.xyz + offset.xyz, meshlet.center.w, 8);
        }
    }

    if (visible)
    {
        uint index = WavePrefixCountBits(visible);
        TaskPayload.MeshletIndices[index] = dtid.x;
        TaskPayload.Offset = offset;
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

VSOutput GetVertexAttributes(uint meshletIndex, uint vertexIndex, float4 offset)
{
    float4 pv = PositionBuf[vertexIndex] + offset;
    ExtVertex ev = ExtBuf[vertexIndex];

    VSOutput output = (VSOutput)0;
    output.WorldPos = mul(World, pv).xyz;
    output.Normal   = mul((float3x3)World, ev.normal.xyz);
    output.Tangent  = ev.tangent;
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
        vertices[gtid] = GetVertexAttributes(meshletIndex, vertexIndex, taskPayload.Offset);
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
    float4 albedo = Albedo * float4(pow(texAlbedo.rgb, 2.2), texAlbedo.a);

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
    float3x3 TBN = float3x3(T, B, N);
    N = normalize(mul(normalize(tNormal), TBN));
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

#if ENABLE_SHADOW
    float4 fragPosLightSpace = mul(biasMat, mul(LightMatrix, float4(input.WorldPos, 1.0)));
    float4 shadowCoord = fragPosLightSpace / fragPosLightSpace.w;
    float shadow = FilterPCF(shadowCoord);
#else
	float shadow = 1.0;
#endif
    float3 e0 = BRDF(V, N, light, pbrParam) * shadow;

#if ENABLE_IBL
    // Calculate view dot normal for Fresnel
    float cosTheta = max(dot(N, V), 0.001);  // Clamp to avoid zero at grazing angle

    // Correct F0 based on metallic
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), pbrParam.Albedo, pbrParam.Metallic);

    // Reflection vector: correct orientation - MUST use -V for proper reflection
    float3 R = reflect(-V, N);

    // Sample prefiltered reflection and BRDF LUT
    float3 prefilteredColor = PrefilteredReflection(R, pbrParam.Roughness);
    float2 brdf = BRDFLut.Sample(BRDFLutSampler, float2(cosTheta, pbrParam.Roughness)).rg;

    // Fresnel-Schlick approximation
    float3 F = F_SchlickR(cosTheta, F0, pbrParam.Roughness);

    // Specular component: properly combine BRDF terms
    // brdf.x = F, brdf.y = (1-F), so: F*brdf.x + brdf.y approximates the full BRDF response
    float3 specular = prefilteredColor * (F * brdf.x + brdf.y);

    // Sample irradiance for diffuse
    float3 irradiance = IrradianceMap.Sample(IrradianceSampler, N).rgb;

    // Diffuse component with metallic influence (metals don't have diffuse)
    float3 kD = (1.0 - F) * (1.0 - pbrParam.Metallic);
    float3 diffuse = irradiance * pbrParam.Albedo * kD;

    // Combine ambient contributions with proper weighting
    // Add minimum brightness to prevent complete darkness at edges
    float3 ambient = (diffuse + specular) * pbrParam.AO;

    // Ensure minimum brightness to avoid edge darkening artifacts
    // This is especially important for grazing angles where N·V is near zero
    float minBrightness = 0.02 * pbrParam.Albedo * pbrParam.AO;  // Subtle minimum fill light
    ambient = max(ambient, minBrightness);

    return float4(e0 + ambient, albedo.a);
#else
    // Fallback without IBL
    float3 ambient = float3(0.03, 0.03, 0.03) * pbrParam.Albedo * pbrParam.AO;
    return float4(e0 + ambient, albedo.a);
#endif
}
//------------------------------------------ Fragment Shader------------------------------------------//