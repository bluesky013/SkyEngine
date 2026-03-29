#define VIEW_COUNT 1

#include "layout/default_pass.hlslh"
#include "lighting/pbr.hlslh"

#if VIEW_COUNT > 1
#define VIEW_INFO View[ViewIndex]
#else
#define VIEW_INFO View
#endif

// ==================== Structures ====================

struct VSInput
{
    uint2  Pos : POSITION;
    uint   ID  : INST0;

    uint InstanceID : SV_InstanceID;
};

struct VSOutput
{
    float2 UV            : UV;
    float3 WorldPos      : POSITION0;
    float3 Normal        : NORMAL;
    float4 ShadowCoord   : POSITION1;

    float4 Pos : SV_POSITION;
};

struct ClipmapBlock {
    float  offsetX;
    float  offsetZ;
    float  scale;
    uint   level;
};

// ==================== Per Batch (space1) ====================

[[vk::binding(0, 1)]] StructuredBuffer<ClipmapBlock> InstancedData : register(t0, space1);

// ==================== Per Instance (space2) ====================

[[vk::binding(0, 2)]] Texture2D    HeightMap        : register(t0, space2);
[[vk::binding(1, 2)]] SamplerState HeightMapSampler : register(s0, space2);
[[vk::binding(2, 2)]] Texture2D    SplatMap         : register(t1, space2);
[[vk::binding(3, 2)]] SamplerState SplatMapSampler  : register(s1, space2);

// ==================== Material constants ====================

cbuffer TerrainMaterial : register(b0, space1)
{
    float HeightScale;
    float HeightOffset;
    float TexelSize;     // 1.0 / heightmap resolution
    float HasSplatMap;   // > 0.5 means splatmap is bound
    float4 BaseColor;    // fallback albedo when no splatmap
    float4 LayerColor0;  // splatmap R channel layer
    float4 LayerColor1;  // splatmap G channel layer
    float4 LayerColor2;  // splatmap B channel layer
    float4 LayerColor3;  // splatmap A channel layer
    float4 LayerRoughness; // roughness per layer (R=layer0, G=layer1, B=layer2, A=layer3)
};

// ==================== Vertex Shader ====================

VSOutput VSMain(VSInput input)
{
    VSOutput output = (VSOutput)0;

    ClipmapBlock block = InstancedData[input.ID];

    // Compute world XZ from block offset + local vertex * scale
    float2 posXZ = float2(
        block.offsetX + float(input.Pos.x) * block.scale,
        block.offsetZ + float(input.Pos.y) * block.scale
    );

    // Compute UV for heightmap sampling
    // TODO: proper UV mapping from world coords to heightmap tile atlas
    float2 uv = float2(float(input.Pos.x) / 64.0, float(input.Pos.y) / 64.0);

    // Sample height at fine grid position
    float h = HeightMap.SampleLevel(HeightMapSampler, uv, 0).r;
    float height = h * HeightScale + HeightOffset;

    // ---- Geomorphing (LOD transition blending) ----
    // Snap the vertex position to the coarser grid (2× spacing) and blend
    // the height toward the coarser sample near the level boundary.
    float coarseScale = block.scale * 2.0;
    float2 coarseXZ = floor(posXZ / coarseScale + 0.5) * coarseScale;
    float2 coarseUV = clamp(uv + (coarseXZ - posXZ) * TexelSize / block.scale, 0.0, 1.0);
    float hCoarse = HeightMap.SampleLevel(HeightMapSampler, coarseUV, 0).r;
    float heightCoarse = hCoarse * HeightScale + HeightOffset;

    // Blend factor: 0 at level center (full detail), 1 at outer boundary (morph to coarser)
    float3 cameraPos = VIEW_INFO.World[3].xyz;
    float distToCamera = length(posXZ - cameraPos.xz);
    // Level covers ~4 blocks per side, each blockSize*scale wide
    float levelRadius = 2.0 * 64.0 * block.scale;
    float morphStart = levelRadius * 0.7;
    float morphEnd   = levelRadius;
    float morphAlpha = saturate((distToCamera - morphStart) / max(morphEnd - morphStart, 0.001));

    height = lerp(height, heightCoarse, morphAlpha);
    // ---- End geomorphing ----

    float3 worldPos = float3(posXZ.x, height, posXZ.y);
    output.WorldPos = worldPos;
    output.UV = uv;

    // Compute normal from heightmap finite differences
    float texelWorld = block.scale;
    float hL = HeightMap.SampleLevel(HeightMapSampler, uv + float2(-TexelSize, 0), 0).r * HeightScale;
    float hR = HeightMap.SampleLevel(HeightMapSampler, uv + float2( TexelSize, 0), 0).r * HeightScale;
    float hD = HeightMap.SampleLevel(HeightMapSampler, uv + float2(0, -TexelSize), 0).r * HeightScale;
    float hU = HeightMap.SampleLevel(HeightMapSampler, uv + float2(0,  TexelSize), 0).r * HeightScale;

    float3 normal = normalize(float3(hL - hR, 2.0 * texelWorld, hD - hU));
    output.Normal = normal;

    // Shadow coord
    output.ShadowCoord = mul(LightMatrix, float4(worldPos, 1.0));

    // Clip space
    output.Pos = mul(VIEW_INFO.ViewProj, float4(worldPos, 1.0));

    return output;
}

// ==================== Shadow helpers ====================

static const float SHADOW_BIAS = 0.005;

float TextureProjTerrain(float4 shadowCoord, float2 off)
{
    float shadow = 1.0;
    if (shadowCoord.z > -1.0 && shadowCoord.z < 1.0) {
        float dist = ShadowMap.Sample(ShadowMapSampler, shadowCoord.xy + off).r;
        if (shadowCoord.w > 0.0 && dist + SHADOW_BIAS < shadowCoord.z) {
            shadow = 0.1;
        }
    }
    return shadow;
}

float FilterPCFTerrain(float4 sc)
{
    float2 texDim;
    ShadowMap.GetDimensions(texDim.x, texDim.y);
    float dx = 1.0 / texDim.x;
    float dy = 1.0 / texDim.y;

    float shadowFactor = 0.0;
    int count = 0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            shadowFactor += TextureProjTerrain(sc, float2(dx * x, dy * y));
            count++;
        }
    }
    return shadowFactor / count;
}

// ==================== IBL helpers ====================

float3 PrefilteredReflectionTerrain(float3 R, float roughness)
{
    const float MAX_REFLECTION_LOD = 9.0;
    float lod = roughness * MAX_REFLECTION_LOD;
    float lodf = floor(lod);
    float lodc = ceil(lod);
    float3 a = PrefilteredMap.SampleLevel(PrefilteredMapSampler, R, lodf).rgb;
    float3 b = PrefilteredMap.SampleLevel(PrefilteredMapSampler, R, lodc).rgb;
    return lerp(a, b, lod - lodf);
}

// ==================== Fragment Shader ====================

float4 FSMain(VSOutput input) : SV_TARGET
{
    float3 N = normalize(input.Normal);

    // Splatmap-based layer blending or fallback
    float3 albedo;
    float  roughness;

    if (HasSplatMap > 0.5)
    {
        float4 splat = SplatMap.Sample(SplatMapSampler, input.UV);
        // Normalize weights (in case they don't sum to 1)
        float totalWeight = splat.r + splat.g + splat.b + splat.a;
        if (totalWeight > 0.001)
            splat /= totalWeight;
        else
            splat = float4(1, 0, 0, 0);

        albedo = LayerColor0.rgb * splat.r
               + LayerColor1.rgb * splat.g
               + LayerColor2.rgb * splat.b
               + LayerColor3.rgb * splat.a;

        roughness = LayerRoughness.r * splat.r
                  + LayerRoughness.g * splat.g
                  + LayerRoughness.b * splat.b
                  + LayerRoughness.a * splat.a;
    }
    else
    {
        albedo = BaseColor.rgb;
        roughness = 0.85;
    }

    float  metallic  = 0.0;   // Terrain is dielectric
    float  ao        = 1.0;

    // Camera / view vector
    float3 viewPos = VIEW_INFO.World[3].xyz;
    float3 V = normalize(viewPos - input.WorldPos);

    // Direct lighting
    LightInfo light;
    light.Direction = MainLightDirection;
    light.Color     = MainLightColor;

    StandardPBR pbrParam;
    pbrParam.Metallic  = metallic;
    pbrParam.Roughness = roughness;
    pbrParam.Albedo    = albedo;
    pbrParam.AO        = ao;
    pbrParam.Emissive  = float4(0, 0, 0, 0);

    // Shadow
    float4 sc = input.ShadowCoord / input.ShadowCoord.w;
    float shadow = FilterPCFTerrain(sc);

    float3 directLight = BRDF(V, N, light, pbrParam) * shadow;

    // IBL
    float cosTheta = max(dot(N, V), 0.001);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 R = reflect(-V, N);

    float3 prefilteredColor = PrefilteredReflectionTerrain(R, roughness);
    float2 brdf = BRDFLut.Sample(BRDFLutSampler, float2(cosTheta, roughness)).rg;
    float3 F = F_SchlickR(cosTheta, F0, roughness);

    float3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    float3 irradiance = IrradianceMap.Sample(IrradianceSampler, N).rgb;
    float3 kD = (1.0 - F) * (1.0 - metallic);
    float3 diffuse = irradiance * albedo * kD;
    float3 ambient = (diffuse + specular) * ao;
    float3 minBrightness = 0.02 * albedo * ao;
    ambient = max(ambient, minBrightness);

    float3 color = directLight + ambient;
    return float4(color, 1.0);
}