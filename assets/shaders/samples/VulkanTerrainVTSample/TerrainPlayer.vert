#version 450

vec2 positions[3] = vec2[](
    vec2(-0.25, -0.25),
    vec2( 0.25, -0.25),
    vec2( 0.0,  0.5)
);

layout (set = 0, binding = 0) uniform Camera {
    mat4 project;
} camera;

layout (set = 1, binding = 0) uniform Local {
    vec4 data;
} local;

void main() {
    float angle = local.data.z;
    mat2 rot = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
    vec2 outPos = rot * positions[gl_VertexIndex];

    gl_Position = camera.project * vec4(outPos * vec2(25.0) + local.data.xy, 0, 1.0);
    gl_Position.y = -gl_Position.y;
}