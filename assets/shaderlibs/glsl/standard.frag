#version 450
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 tangent;
layout (location = 3) in vec4 color;
layout (location = 4) in vec2 uv;

layout (location = 0) out vec4 outFragColor;

#include <shaderlibs/glsl/layouts/standard_perpass.glsl>

layout (set = 1, binding = 0) uniform MaterialInfo {
    vec4 baseColor;
    float metallic;
    float roughness;
    float alphaCutoff;
    bool useMask;
    bool useBaseColorMap;
    bool useNormalMap;
    bool useEmissiveMap;
    bool useAOMap;
    bool useMetallicRoughnessMap;
} material;

layout (set = 1, binding = 1) uniform sampler2D baseColorMap;
layout (set = 1, binding = 2) uniform sampler2D normalMap;
layout (set = 1, binding = 3) uniform sampler2D emissiveMap;
layout (set = 1, binding = 4) uniform sampler2D aoMap;
layout (set = 1, binding = 5) uniform sampler2D metallicRoughnessMap;

const float PI = 3.1415926;

float TrowbridgeReitz(float NdotH, float alpha2)
{
    float v = NdotH > 0 ? 1.0 : 0.0;
    float tmp = v * NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / (PI * tmp * tmp);
}

float GSmith(float v, float alpha2)
{
    return v / (v + sqrt(alpha2 + (1.0 - alpha2) * v * v));
}

float Schlicksmith(float NdotL, float NdotV, float alpha2)
{
    float g1 = GSmith(abs(NdotL), alpha2);
    float g2 = GSmith(abs(NdotV), alpha2);
    return 4 * g1 * g2;
}

vec3 SchlickFresnel(vec3 baseColor, float metallic, float VdotH)
{
    vec3 f0 = mix(vec3(0.04), baseColor, metallic);
    return f0 + (1 - f0) * pow(1 - abs(VdotH), 5.0);
}

vec3 CalculateBRDF(vec3 L, vec3 V, vec3 N, float metallic, float roughness, vec3 baseColor, vec3 lightColor)
{
    vec3 H = normalize (V + L);

    float NdotV = dot(N, V);
    float NdotL = dot(N, L);
    float NdotH = dot(N, H);
    float VdotH = dot(V, H);

    vec3 cDiff = mix(baseColor, vec3(0.0), metallic);

    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;

    vec3 F = SchlickFresnel(baseColor, metallic, VdotH);
    float D = TrowbridgeReitz(NdotH, alpha2);
    float G = Schlicksmith(NdotL, NdotV, alpha2);

    vec3 diffuse = (1 - F) * cDiff / PI;
    vec3 specular = F * D * G / (4 * abs(NdotL) * abs(NdotV));
    return lightColor * (diffuse + specular);
}


struct Light {
    vec4 color;
    vec3 dir;
    float intensity;
};

void main()
{
    Light light;
    light.color = vec4(1.0, 1.0, 1.0, 1.0);
    light.dir = normalize(vec3(-1, -1, -1));

    vec3 N = normalize(normal);

    vec4 baseColor = color * material.baseColor;
    if (material.useBaseColorMap) {
        vec4 colorFromTex = texture(baseColorMap, uv);
        baseColor = vec4(pow(colorFromTex.rgb, vec3(2.2)), colorFromTex.a) * baseColor;
    }

    if (material.useMask && baseColor.a < material.alphaCutoff) {
        discard;
    }

    if (material.useNormalMap) {
        vec3 tNormal = normalize(texture(normalMap, uv).xyz * 2.0 - 1.0);
        vec3 T = normalize(tangent.xyz);
        vec3 B = cross(N, tangent.xyz) * (tangent.w > 0.0 ? 1.0 : -1.0);
        mat3 TBN = mat3(T, B, N);

        N = normalize(TBN * tNormal);
    }

    vec3 viewPos = viewInfo.worldPos.xyz;
    vec3 L = -light.dir;
    vec3 V = normalize(viewPos - pos);

    float metallic = material.metallic;
    float roughness = material.roughness;
    if (material.useMetallicRoughnessMap) {
        vec4 mr = texture(metallicRoughnessMap, uv);
        metallic = mr.b;
        roughness = mr.g;
    }

    // brdf
    vec3 outColor = CalculateBRDF(L, V, N, metallic, roughness, baseColor.rgb, light.color.rgb);

    if (material.useAOMap) {
//        float ao = texture(aoMap, uv).r;
//        outColor = outColor * ao;
    }

    if (material.useEmissiveMap) {
        vec3 emissive = texture(emissiveMap, uv).rgb;
        outColor += emissive * 1.5;
    }

//    outColor = pow(outColor, vec3(1.0f / 2.2));
    outFragColor = vec4(outColor, baseColor.a);
}
