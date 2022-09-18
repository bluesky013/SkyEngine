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

void main()
{
    // outFragColor = vec4(0.92, 0.11, 0.92, 1.0);
    outFragColor = material.baseColor;
}