#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 color0;
layout(location = 1) out vec4 color1;
layout(location = 2) out vec4 color2;
layout(location = 3) out vec4 color3;

void main()
{
    color0 = vec4(uv.x, 0, 0, 0.5);
    color1 = vec4(0, uv.y, 0, 0.5);
    color2 = vec4(1 - uv.x, 0, 0, 0.5);
    color3 = vec4(0, 1 - uv.y, 0, 0.5);
}