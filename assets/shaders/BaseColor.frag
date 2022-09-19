#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 color;
layout (location = 3) in vec2 uv;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 1) uniform SceneInfo {
    int lightCount;
} sceneInfo;

layout (set = 2, binding = 0) uniform MaterialInfo {
    vec4 baseColor;
} material;

layout (set = 2, binding = 1) uniform sampler2D baseColor;

void main()
{
    vec3 L = normalize(vec3(0, 20, 0) - pos);
    vec3 N = normalize(normal);
    float nDotL = clamp(dot(N, L), 0, 1);
    outFragColor = color * material.baseColor * nDotL;
}
