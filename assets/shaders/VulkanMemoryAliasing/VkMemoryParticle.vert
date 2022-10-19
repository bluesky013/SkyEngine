#version 450

layout(location = 0) in vec4 inPos;

layout(location = 0) out vec2 outUv;

vec2 positions[6] = vec2[](
vec2(-0.015, -0.015),
vec2( 0.015, -0.015),
vec2( 0.015,  0.015),
vec2( 0.015,  0.015),
vec2(-0.015,  0.015),
vec2(-0.015, -0.015)
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
    vec2 pos = positions[gl_VertexIndex];
    float angle = inPos.z;

    mat2 rot = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
    vec2 outPos = rot * pos;

    gl_Position = vec4(outPos + inPos.xy, 0.0, 1.0);
    gl_Position.y = -gl_Position.y;
    outUv = uv[gl_VertexIndex];
}
