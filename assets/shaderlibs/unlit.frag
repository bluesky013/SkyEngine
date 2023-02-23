#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 tangent;
layout (location = 3) in vec3 biTangent;
layout (location = 4) in vec4 color;
layout (location = 5) in vec2 uv;

layout (location = 0) out vec4 outFragColor;

void main()
{
    vec3 L = normalize(vec3(0, 20, 0) - pos);
    vec3 N = normalize(normal);
    float nDotL = clamp(dot(N, L), 0, 1);
    outFragColor = color * nDotL;
}
