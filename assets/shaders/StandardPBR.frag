#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 tangent;
layout (location = 3) in vec3 biTangent;
layout (location = 4) in vec4 color;
layout (location = 5) in vec2 uv;

layout (location = 0) out vec4 outFragColor;

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

void main()
{
    vec3 L = normalize(vec3(0, 20, 0) - pos);
    vec3 N = normalize(normal);
    vec3 T = normalize(tangent);
    vec3 B = normalize(biTangent);
    mat3 TBN = mat3(T, B, N);

    vec3 tNormal = normalize(texture(normalMap, uv).xyz * 2.0 - 1.0);
    N = normalize(TBN * tNormal);

    float nDotL = clamp(dot(N, L), 0, 1);

    vec4 baseColor = texture(baseColorMap, uv) * color;
    vec4 emissive = texture(emissiveMap, uv);
    float ao = texture(lightMap, uv).r;

    outFragColor = baseColor * material.baseColor * nDotL * ao + emissive;
}
