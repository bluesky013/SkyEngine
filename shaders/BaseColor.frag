#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 color;
layout (location = 3) in vec2 uv;

layout (location = 0) out vec4 outFragColor;

layout (set = 1, binding = 1) uniform SceneInfo {
    int lightCount;
} sceneInfo;

void main()
{
    outFragColor = color;
}