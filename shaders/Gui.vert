#version 450 core

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inUv;
layout (location = 2) in vec4 inColor;
layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outUv;

layout (set = 0, binding = 0) uniform UIInfo {
    vec2 scale;
    vec2 translate;
} pc;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main(void)
{
    outColor = inColor;
    outUv = inUv;
    gl_Position = vec4(inPos * pc.scale + pc.translate, 0, 1);
}