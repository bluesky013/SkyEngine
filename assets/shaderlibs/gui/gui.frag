#version 450

layout (location = 0) in vec4 color;
layout (location = 1) in vec2 uv;

layout (location = 0) out vec4 outFragColor;

layout (set = 1, binding = 1) uniform sampler2D fontTexture;

void main()
{
    outFragColor = color * texture(fontTexture, uv);
}
