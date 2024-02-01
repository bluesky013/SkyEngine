#include "shaders/common/Constants.hlsl"

float D_GGX(float dotNH, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
    return (alpha2)/(PI * denom * denom);
}

float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    float GL = dotNL / (dotNL * (1.0 - k) + k);
    float GV = dotNV / (dotNV * (1.0 - k) + k);
    return GL * GV;
}

float3 F_Schlick(float cosTheta, float metallic, float3 elbedo)
{
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), elbedo, metallic);
    float3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
    return F;
}

struct LightInfo {
    float4 Direction; // xyz: dir
    float4 Color;     // xyz: rgb
};

struct StandardPBR {
    float Metallic;
    float Roughness;
    float4 Albedo;
};

float3 BRDF(float3 V, float3 N, LightInfo light, StandardPBR param)
{
    float3 L = normalize(light.Direction.xyz);
    float3 H = normalize(V + L);
    float dotNV = clamp(dot(N, V), 0.0, 1.0);
    float dotNL = clamp(dot(N, L), 0.0, 1.0);
    float dotLH = clamp(dot(L, H), 0.0, 1.0);
    float dotNH = clamp(dot(N, H), 0.0, 1.0);

    float3 color = float3(0.0, 0.0, 0.0);
    if (dotNL > 0.0)
    {
        float rroughness = max(0.05, param.Roughness);
        float D = D_GGX(dotNH, param.Roughness);
        float G = G_SchlicksmithGGX(dotNL, dotNV, rroughness);
        float3 F = F_Schlick(dotNV, param.Metallic, param.Albedo.xyz);

        float3 spec = D * F * G / (4.0 * dotNL * dotNV);
        color += spec * dotNL * light.Color.rgb;
    }

    return color;
}