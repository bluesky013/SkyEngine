struct ViewInfo {
    float4x4 ViewToWorld;
    float4x4 ViewToClip;
    float4x4 WorldToView;
    float4x4 WorldToClip;
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