#version 450

layout (location = 0) in vec2 inUv;
layout (location = 1) in flat int inLevel;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D sampler0;

void main()
{
    outColor = texture(sampler0, inUv);
    outColor = vec4(pow(outColor.xyz, vec3(1.0f / 2.2)), outColor.w);
}
