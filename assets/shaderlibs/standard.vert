#version 450 core

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec4 inNormal;
layout (location = 2) in vec4 inTangent;
layout (location = 3) in vec4 inBiTangent;
layout (location = 4) in vec4 inColor;
layout (location = 5) in vec2 inUv;

layout (location = 0) out vec3 outPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 outTangent;
layout (location = 3) out vec3 outBiTangent;
layout (location = 4) out vec4 outColor;
layout (location = 5) out vec2 outUv;

layout (set = 0, binding = 0) uniform ViewInfo {
    mat4 viewToWorldMatrix;
    mat4 worldToViewMatrix;
    mat4 viewToClipMatrix;
    mat4 worldToClipMatrix;
    vec3 position;
} viewInfo;

layout (set = 1, binding = 0) uniform ObjectInfo {
    mat4 worldMatrix;
    mat4 inverseTranspose;
} objectInfo;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    vec4 worldPos = objectInfo.worldMatrix * inPos;
    gl_Position = viewInfo.worldToClipMatrix * worldPos;

    outPos = worldPos.xyz / worldPos.w;
    outNormal = mat3(objectInfo.inverseTranspose) * inNormal.xyz;
    outTangent = normalize(mat3(objectInfo.inverseTranspose) * inTangent.xyz);
    outBiTangent = normalize(mat3(objectInfo.inverseTranspose) * inBiTangent.xyz);
    outColor = inColor;
    outUv = inUv;
}