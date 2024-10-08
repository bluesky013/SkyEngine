#include "shaders/common/constants.hlsl"
#include "shaders/common/random.hlsl"

float2 Hammersley2d(uint i, uint N)
{
    // Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
    uint bits = (i << 16u) | (i >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    float rdi = float(bits) * 2.3283064365386963e-10;
    return float2(float(i) /float(N), rdi);
}

// Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
float3 ImportanceSample_GGX(float2 Xi, float roughness, float3 normal)
{
    // Maps a 2D point to a hemisphere with spread based on roughness
    float alpha = roughness * roughness;
    float phi = 2.0 * PI * Xi.x + Random(normal.xz) * 0.1;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (alpha*alpha - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float3 H = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    // Tangent space
    float3 up = abs(normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangentX = normalize(cross(up, normal));
    float3 tangentY = normalize(cross(normal, tangentX));

    // Convert to world Space
    return normalize(tangentX * H.x + tangentY * H.y + normal * H.z);
}

#include "shaders/lighting/light.hlsl"

float3 DiffuseBRDF(float3 albedo)
{
    return (1 / PI) * albedo;
}

float3 F_Schlick(float dotVH, float3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - dotVH, 5.0);
}

float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2) / (PI * denom * denom);
}

float G_SchlickGGX(float dotNV, float roughness)
{
     float r = (roughness + 1.0);
     float k = (r*r) / 8.0;

     float num   = dotNV;
     float denom = dotNV * (1.0 - k) + k;
     return num / denom;
}

float G_SchlickSmithGGX(float dotNL, float dotNV, float roughness)
{
    float ggx1 = G_SchlickGGX(dotNL, roughness);
    float ggx2 = G_SchlickGGX(dotNV, roughness);
    return ggx1 * ggx2;
}

struct StandardPBR {
    float Metallic;
    float Roughness;
    float3 Albedo;
    float AO;
    float4 Emissive;
};

float3 BRDF(float3 V, float3 N, LightInfo light, StandardPBR param)
{
    float3 L = normalize(-light.Direction.xyz);
    float3 H = normalize(V + L);
    float dotNV = max(dot(N, V), 0);
    float dotNL = max(dot(N, L), 0);
    float dotVH = max(dot(V, H), 0);
    float dotNH = max(dot(N, H), 0);

    float3 L0 = float3(0.0, 0.0, 0.0);

    float3 F0 = lerp(0.04, param.Albedo, param.Metallic);

    float3 F = F_Schlick(dotVH, F0);
    float D = D_GGX(dotNH, param.Roughness);
    float G = G_SchlickSmithGGX(dotNL, dotNV, param.Roughness);

    float3 specular = D * F * G / 4 * dotNV * dotNL;

    float3 kD = lerp(param.Albedo, 0, param.Metallic);
    L0 += ((1 - F) * kD / PI + specular) * light.Direction.w * param.AO + param.Emissive;
    return L0;
}