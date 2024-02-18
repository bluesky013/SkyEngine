#version 450 core
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec4 inUv;
layout (location = 2) in vec4 inNormal;
layout (location = 3) in vec4 inTangent;
layout (location = 4) in vec4 inColor;

layout (location = 0) out vec3 outPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec4 outTangent;
layout (location = 3) out vec4 outColor;
layout (location = 4) out vec2 outUv;

#include <shaderlibs/glsl/layouts/standard_perpass.glsl>
#include <shaderlibs/glsl/layouts/standard_local_vs.glsl>

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
    outTangent = vec4(normalize(mat3(localData.inverseTranspose) * inTangent.xyz), inTangent.w);
    outColor = inColor;
    outUv = inUv.xy;
}