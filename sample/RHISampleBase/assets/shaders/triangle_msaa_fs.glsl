#version 450 core

layout(location = 0) in vec4 inColor;
layout(location = 0) out vec4 outColor1;
layout(location = 1) out vec4 outColor2;

void main()
{
    outColor1 = inColor;
    outColor2 = vec4(vec3(1.0) - inColor.xyz, inColor.w);
}