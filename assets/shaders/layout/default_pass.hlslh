struct ViewInfo {
    float4x4 World;
    float4x4 View;
    float4x4 Project;
    float4x4 ViewProj;
};

[[vk::binding(0, 0)]] cbuffer global : register(b0, space0)
{
    float4x4 LightMatrix;
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

// [[vk::binding(2, 0)]] Texture2D ShadowMap : register(t1, space0);
// [[vk::binding(3, 0)]] SamplerState ShadowMapSampler : register(t1, space0);