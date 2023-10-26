#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 tangent;
layout (location = 3) in vec4 color;
layout (location = 4) in vec2 uv;

layout (location = 0) out vec4 outFragColor;

#include <shaderlibs/layouts/standard_perpass.glsl>

layout (set = 1, binding = 0) uniform sampler2D mainColor;


void main()
{
    outFragColor = texture(mainColor, uv);
}
