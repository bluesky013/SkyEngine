#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUv;

layout (location = 0) out vec4 outFragColor;

layout (set = 2, binding = 0) uniform sampler2D baseColor;

void main()
{
    outFragColor = texture(baseColor, inUv);
}