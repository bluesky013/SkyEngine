#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 color;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 2) uniform LightInfo {
    vec4 lightPos;
} lightInfo;

void main()
{
    outFragColor = color;
}