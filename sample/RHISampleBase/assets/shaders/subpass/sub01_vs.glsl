#version 450

layout(location = 0) out vec2 outUv;

vec3 positions[3] = vec3[](
vec3(-1.0,  3.0, 1),
vec3(-1.0, -1.0, 0),
vec3( 3.0, -1.0, 1)
);

vec2 uv[3] = vec2[](
vec2(0.0, 2.0),
vec2(0.0, 0.0),
vec2(2.0, 0.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 1.0);
    outUv = uv[gl_VertexIndex];
}