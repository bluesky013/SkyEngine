[[vk::binding(0, 2)]] cbuffer local : register(b0, space2)
{
    float4x4 World;
    float4x4 InverseTrans;
}

#if ENABLE_SKIN

#define MAX_BONE_NUM (80)
[[vk::binding(1, 2)]] cbuffer skin : register(b1, space2)
{
    float4x4 Bones[MAX_BONE_NUM];
}

#endif