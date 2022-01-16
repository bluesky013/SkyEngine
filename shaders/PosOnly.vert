#version 450 core

layout (location = 0) in vec4 inPos;

layout (location = 0) out vec3 outPos;

layout (set = 0, binding = 0) uniform ObjectInfo {
    mat4 worldMatrix;
    mat4 inverseTranspose;
} objectInfo;

layout (set = 1, binding = 0) uniform ViewInfo {
    mat4 viewProject;
} viewInfo;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main(void)
{
    vec4 worldPos = objectInfo.worldMatrix * inPos;
    gl_Position = viewInfo.viewProject * worldPos;
    outPos = worldPos.xyz / worldPos.w;
}
