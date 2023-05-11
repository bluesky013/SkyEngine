#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 color0;
layout(location = 1) out vec4 color1;

void main()
{
    color0 = vec4(uv.x, 0, 0, 0.5);
    color1 = vec4(0, uv.y, 0, 0.5);
}