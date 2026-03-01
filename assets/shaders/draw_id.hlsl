struct VSInput
{
    float3 Pos   : POSITION;
};

struct VSOutput
{
    float4 Pos   : SV_POSITION;
    nointerpolation uint ID : DrawID;
};

struct ViewInfo {
    float4x4 World;
    float4x4 View;
    float4x4 ViewProj;
    float4x4 LastViewProj;
};

#if VIEW_COUNT > 1
[[vk::binding(0, 0)]] cbuffer view   : register(b0, space0)
{
    ViewInfo View[VIEW_COUNT];
}
#else
[[vk::binding(0, 0)]] cbuffer view   : register(b0, space0)
{
    ViewInfo View;
}
#endif

#if VIEW_COUNT > 1
#define VIEW_INFO View[ViewIndex]
#else
#define VIEW_INFO View
#endif

[[vk::binding(0, 1)]] cbuffer Batch : register(b0, space1)
{
    float4x4 World;
    uint ID;
}

VSOutput VSMain(VSInput input)
{
    VSOutput output = (VSOutput)0;

    float4 WorldPos = mul(World, float4(input.Pos, 1.0));
    output.Pos = mul(VIEW_INFO.ViewProj, WorldPos);
    output.ID = ID;
    return output;
}

uint FSMain(VSOutput input) : SV_TARGET
{
    return input.ID;
}