#include "shaders/lighting/pbr.hlsl"

[[vk::binding(0, 1)]] cbuffer shading : register(b0, space1) {
    float Metallic;
    float Roughness;
    float AlphaCutoff;
    float4 Albedo;
}

[[vk::binding(1, 1)]] Texture2D AlbedoMap            : register(t0, space1);
[[vk::binding(2, 1)]] Texture2D NormalMap            : register(t1, space1);
[[vk::binding(3, 1)]] Texture2D AoMap                : register(t2, space1);
[[vk::binding(4, 1)]] Texture2D MetallicRoughnessMap : register(t3, space1);
[[vk::binding(5, 1)]] Texture2D EmissiveMap          : register(t4, space1);

[[vk::binding(6, 1)]] SamplerState AlbedoSampler            : register(s0, space1);
[[vk::binding(7, 1)]] SamplerState NormalSampler            : register(s1, space1);
[[vk::binding(8, 1)]] SamplerState AoSampler                : register(s2, space1);
[[vk::binding(9, 1)]] SamplerState MetallicRoughnessSampler : register(s3, space1);
[[vk::binding(10, 1)]] SamplerState EmissiveSampler          : register(s4, space1);
