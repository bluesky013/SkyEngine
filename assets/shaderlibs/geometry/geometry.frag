#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec4 color;
layout (location = 2) in vec3 normal;

layout (location = 0) out vec4 outFragColor;

void main()
{
    vec3 L = normalize(vec3(-1, -1, -1));
    vec3 N = normalize(normal);
    float nDotL = clamp(dot(N, L), 0, 1);

    outFragColor = color * nDotL;
}
