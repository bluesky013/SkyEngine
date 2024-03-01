struct ViewInfo {
    float4x4 World;
    float4x4 View;
    float4x4 Project;
    float4x4 ViewProj;
};

#define VIEW_COUNT 1
[[vk::binding(0, 0)]] cbuffer global : register(b0, space0)
{
    float4 Viewport;
}

#if VIEW_COUNT > 1
[[vk::binding(1, 0)]] cbuffer view   : register(b1, space0)
{
    ViewInfo View[VIEW_COUNT];
}
#else
[[vk::binding(1, 0)]] cbuffer view   : register(b1, space0)
{
    ViewInfo View;
}
#endif