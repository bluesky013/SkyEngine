#define VIEW_COUNT 1

#include "layout/default_pass.hlslh"

#if VIEW_COUNT > 1
#define VIEW_INFO View[ViewIndex]
#else
#define VIEW_INFO View
#endif

struct VSInput
{
    uint2  Pos : POSITION;
    uint   ID  : INST0;

    uint InstanceID : SV_InstanceID;
};

struct VSOutput
{
    float4 Pos : SV_POSITION;
};

struct ClipmapBlock {
    float  offsetX;
    float  offsetZ;
    float  scale;
    uint   level;
};

[[vk::binding(0, 1)]] StructuredBuffer<ClipmapBlock> InstancedData : register(t0, space1);
[[vk::binding(0, 2)]] Texture2D    HeightMap        : register(t0, space2);
[[vk::binding(1, 2)]] SamplerState HeightMapSampler : register(s0, space2);

cbuffer TerrainMaterial : register(b0, space1)
{
    float HeightScale;
    float HeightOffset;
    float TexelSize;
    float pad0;
    float4 BaseColor;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output = (VSOutput)0;

    ClipmapBlock block = InstancedData[input.ID];

    float2 posXZ = float2(
        block.offsetX + float(input.Pos.x) * block.scale,
        block.offsetZ + float(input.Pos.y) * block.scale
    );

    float2 uv = float2(float(input.Pos.x) / 64.0, float(input.Pos.y) / 64.0);
    float h = HeightMap.SampleLevel(HeightMapSampler, uv, 0).r;
    float height = h * HeightScale + HeightOffset;

    // Geomorphing
    float coarseScale = block.scale * 2.0;
    float2 coarseXZ = floor(posXZ / coarseScale + 0.5) * coarseScale;
    float2 coarseUV = clamp(uv + (coarseXZ - posXZ) * TexelSize / block.scale, 0.0, 1.0);
    float heightCoarse = HeightMap.SampleLevel(HeightMapSampler, coarseUV, 0).r * HeightScale + HeightOffset;

    float3 cameraPos = VIEW_INFO.World[3].xyz;
    float distToCamera = length(posXZ - cameraPos.xz);
    float levelRadius = 2.0 * 64.0 * block.scale;
    float morphAlpha = saturate((distToCamera - levelRadius * 0.7) / max(levelRadius * 0.3, 0.001));
    height = lerp(height, heightCoarse, morphAlpha);

    float3 worldPos = float3(posXZ.x, height, posXZ.y);
    output.Pos = mul(VIEW_INFO.ViewProj, float4(worldPos, 1.0));

    return output;
}

float4 FSMain() : SV_TARGET
{
    return float4(0.0, 0.0, 0.0, 0.0);
}
