#version 450

layout(location = 0) in vec4 inPos;
layout(location = 1) in vec4 inNormal;

layout(location = 0) out vec3 outPos;
layout(location = 1) out vec3 outNormal;

layout(set = 0, binding = 0) uniform CameraData {
    mat4 viewProject;
    vec4 position;
} cam;

layout(set = 2, binding = 0) uniform LocalData {
    mat4 world;
} local;

void main() {
    vec4 pos = cam.viewProject * local.world * vec4(inPos.xyz, 1.0);
    outNormal = mat3(local.world) * normalize(inNormal).xyz;
    outPos = pos.xyz / pos.w;
    gl_Position = pos;
}