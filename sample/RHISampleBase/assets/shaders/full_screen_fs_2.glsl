#version 450

layout(set = 0, binding = 0) uniform sampler2D colorImage1;
layout(set = 0, binding = 1) uniform sampler2D colorImage2;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(colorImage1, uv) + texture(colorImage2, vec2(uv.x, 1 - uv.y));
}