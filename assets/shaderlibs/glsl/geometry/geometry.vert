#version 450 core
#extension GL_GOOGLE_include_directive : enable

#include <shaderlibs/glsl/layouts/standard_perpass.glsl>
#include <shaderlibs/glsl/layouts/standard_local_vs.glsl>

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec4 inNormal;
layout (location = 2) in vec4 inColor;

layout (location = 0) out vec3 outPos;
layout (location = 1) out vec4 outColor;
layout (location = 2) out vec3 outNormal;

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
