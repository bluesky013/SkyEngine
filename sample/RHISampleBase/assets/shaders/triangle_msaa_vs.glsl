#version 450

layout(location = 0) out vec4 outColor;

vec2 positions[3] = vec2[](
vec2(-0.5,  -0.5),
vec2( 0.5,  -0.5),
vec2( 0.0,   0.5)
);

vec4 color[3] = vec4[](
vec4(1.0, 0.0, 0.0, 1.0),
vec4(0.0, 1.0, 0.0, 1.0),
vec4(0.0, 0.0, 1.0, 1.0)
);

void main() {
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, pos.y + 0.5, 1.0);
    outColor = color[gl_VertexIndex];
}