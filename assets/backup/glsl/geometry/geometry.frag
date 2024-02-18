#version 450
#extension GL_GOOGLE_include_directive : enable

#include <shaderlibs/glsl/layouts/standard_perpass.glsl>

layout (location = 0) in vec3 pos;
layout (location = 1) in vec4 color;
layout (location = 2) in vec3 normal;

layout (location = 0) out vec4 outFragColor;

void main()
{
    vec3 L = normalize(vec3(0, 20, 0) - pos);
    vec3 N = normalize(normal);
    float nDotL = clamp(dot(N, L), 0.1, 1);

    outFragColor = nDotL * color;
}
