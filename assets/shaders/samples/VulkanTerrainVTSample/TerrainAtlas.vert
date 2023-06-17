#version 450

layout(location = 0) out vec2 outUv;

vec2 positions[6] = vec2[](
    vec2(-0.5, -0.5),
    vec2( 0.5, -0.5),
    vec2( 0.5,  0.5),

    vec2( 0.5,  0.5),
    vec2(-0.5,  0.5),
    vec2(-0.5, -0.5)
);

vec2 uv[6] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),

    vec2(1.0, 1.0),
    vec2(0.0, 1.0),
    vec2(0.0, 0.0)
);

layout (set = 0, binding = 0) uniform Camera {
    mat4 project;
} camera;

layout (set = 2, binding = 0) uniform LocalData {
    vec4 ext;
} local;

void main() {
    gl_Position = camera.project * vec4(positions[gl_VertexIndex] * local.ext.xy, 0, 1.0);
    outUv = uv[gl_VertexIndex];
}
