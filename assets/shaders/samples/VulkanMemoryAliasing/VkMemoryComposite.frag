#version 450

layout (location = 0) in vec2 inUv;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform sampler2D sampler0;
layout (set = 0, binding = 1) uniform sampler2D sampler1;

void main()
{
    outFragColor = vec4(texture(sampler0, inUv).rgb * 0.8 + texture(sampler1, inUv).rgb * 0.2, 1.0);
}
