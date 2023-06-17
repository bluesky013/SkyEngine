#version 450


layout (location = 0) out vec2 outUv;
layout (location = 1) out flat int level;

vec2 positions[6] = vec2[](
vec2(-0.5, -0.2),
vec2( 0.5, -0.2),
vec2( 0.5,  0.2),
vec2( 0.5,  0.2),
vec2(-0.5,  0.2),
vec2(-0.5, -0.2)
);

vec2 uv[6] = vec2[](
vec2(0.0, 0.0),
vec2(1.0, 0.0),
vec2(1.0, 1.0),

vec2(1.0, 1.0),
vec2(0.0, 1.0),
vec2(0.0, 0.0)
);

void main() {

    gl_Position = vec4((positions[gl_VertexIndex] + vec2(0, gl_InstanceIndex * 0.5)) * (1 / (gl_InstanceIndex + 1.0)), 0.0, 1.0);
    level = gl_InstanceIndex;
    outUv = uv[gl_VertexIndex];
}
