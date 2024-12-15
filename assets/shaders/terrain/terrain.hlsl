#define VIEW_COUNT 1

#include "shaders/layout/default_pass.hlsl"

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
    float2 UV       : UV;
    float3 WorldPos : POSITION;

    float4 Pos : SV_POSITION;
};

struct TerrainInstanced {
    float2 sectorOffset;
    uint2  sectorSize;
    float2 minMaxHeight;
    float  resolution;
    float  padding;
};

// Per Batch
[[vk::binding(0, 1)]] StructuredBuffer<TerrainInstanced> InstancedData : register(t0, space1);

// Per Instance
[[vk::binding(0, 2)]] Texture2D    HeightMap        : register(t0, space2);
[[vk::binding(1, 2)]] SamplerState HeightMapSampler : register(s0, space2);

VSOutput VSMain(VSInput input)
{
    VSOutput output = (VSOutput)0;

    TerrainInstanced instanceData = InstancedData[input.ID];

    float2 uv = float2(float(input.Pos.x) / float(instanceData.sectorSize.x), float(input.Pos.y) / float(instanceData.sectorSize.y));
    float2 posXZ = instanceData.sectorOffset + float2(input.Pos.x * instanceData.resolution, input.Pos.y * instanceData.resolution);
    float height = HeightMap.Sample(HeightMapSampler, uv).r * 50.f;

    float4 pos = float4(posXZ.x, height, posXZ.y, 1.0);
    output.WorldPos = pos.xyz;
    output.UV = uv;

    output.Pos = mul(VIEW_INFO.ViewProj, float4(output.WorldPos, 1.0));

    return output;
}

float4 FSMain(VSOutput input) : SV_TARGET
{
    return float4(0, 0, 0, 0);
}