#include "shaders/lighting/pbr.hlsl"

cbuffer shading : register(b0, space1) {
    float Metallic;
    float Roughness;
    float Test[2][4];
    float4 Albedo;
}

Texture2D AlbedoMap            : register(t0, space1);
Texture2D NormalMap            : register(t1, space1);
Texture2D AoMap                : register(t2, space1);
Texture2D MetallicRoughnessMap : register(t3, space1);
Texture2D EmissiveMap          : register(t4, space1);

SamplerState AlbedoSampler            : register(s0, space1);
SamplerState NormalSampler            : register(s1, space1);
SamplerState AoSampler                : register(s2, space1);
SamplerState MetallicRoughnessSampler : register(s3, space1);
SamplerState EmissiveSampler          : register(s4, space1);
