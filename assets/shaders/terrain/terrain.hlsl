#define VIEW_COUNT 1

struct VSInput
{
    uint8 Pos : POSITION;
};

struct VSOutput
{
    float4 Pos : SV_POSITION;
};

[[vk::binding(0, 2)]] cbuffer local : register(b0, space2)
{
    float4x4 World;
    float4x4 InverseTrans;
}

struct TerrainInstanced {
    uint32 position;
};

[[vk::binding(0, 0)]] StructuredBuffer<TerrainInstanced> InstancedData : register(t0, space1);

[[vk::binding(1, 2)]] Texture2D    HeightMap        : register(t0, space2);
[[vk::binding(2, 2)]] SamplerState HeightMapSampler : register(s0, space2);

VSOutput VSMain(VSInput input)
{
    VSOutput output = (VSOutput)0;

    return output;
}

float4 FSMain(VSOutput input) : SV_TARGET
{
    return float4(0, 0, 0, 0);
}