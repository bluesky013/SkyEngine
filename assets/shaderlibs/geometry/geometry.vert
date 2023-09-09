#version 450 core

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec4 inNormal;
layout (location = 2) in vec4 inColor;

layout (location = 0) out vec3 outPos;
layout (location = 1) out vec4 outColor;
layout (location = 2) out vec3 outNormal;

#include <shaderlibs/layouts/standard_perpass.glsl>
#include <shaderlibs/layouts/standard_local_vs.glsl>

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    vec4 worldPos = localData.worldMatrix * inPos;
    gl_Position = viewInfo.worldToClipMatrix * worldPos;
    outPos = worldPos.xyz / worldPos.w;
    outNormal = mat3(localData.inverseTranspose) * inNormal.xyz;
    outColor = inColor;
}
