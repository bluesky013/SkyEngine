#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 tangent;
layout (location = 3) in vec3 biTangent;
layout (location = 4) in vec4 color;
layout (location = 5) in vec2 uv;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform ViewInfo {
    mat4 viewToWorldMatrix;
    mat4 worldToViewMatrix;
    mat4 viewToClipMatrix;
    mat4 worldToClipMatrix;
    vec3 position;
} viewInfo;

layout (set = 0, binding = 1) uniform SceneInfo {
    int lightCount;
} sceneInfo;

layout (set = 2, binding = 0) uniform MaterialInfo {
    vec4 baseColor;
    float metallic;
    float roughness;
} material;

layout (set = 2, binding = 1) uniform sampler2D baseColorMap;
layout (set = 2, binding = 2) uniform sampler2D normalMap;
layout (set = 2, binding = 3) uniform sampler2D emissiveMap;
layout (set = 2, binding = 4) uniform sampler2D lightMap;
layout (set = 2, binding = 5) uniform sampler2D metallicRoughnessMap;

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
    return lightColor * (diffuse + specular) * 2.5;
}


struct Light {
    vec4 color;
    vec4 pos;
};

void main()
{
    Light light;
    light.color = vec4(1.0, 1.0, 1.0, 1.0);
    light.pos = vec4(0, 20, 0, 1.0);

    vec3 N = normalize(normal);
    vec3 T = normalize(tangent);
    vec3 B = normalize(biTangent);
    mat3 TBN = mat3(T, B, N);

    vec4 baseColor = pow(texture(baseColorMap, uv), vec4(2.2)) * color * material.baseColor;

    vec3 tNormal = normalize(texture(normalMap, uv).xyz * 2.0 - 1.0);
    N = normalize(TBN * tNormal);
    vec3 L = normalize(light.pos.xyz - pos);
    vec3 V = normalize(viewInfo.position - pos);

    vec4 mr = texture(metallicRoughnessMap, uv);
    float metallic = mr.b;
    float roughness = mr.g;

    // brdf
    vec3 color = CalculateBRDF(L, V, N, metallic, roughness, baseColor.rgb, light.color.rgb);

    vec3 emissive = texture(emissiveMap, uv).rgb;
    float ao = texture(lightMap, uv).r;

    vec3 outColor = color * ao + emissive;
    outColor = pow(outColor, vec3(1.0f / 2.2));

    outFragColor = vec4(outColor, 1.0);
}
