#version 450

layout(location = 0) out vec4 outColor;

vec2 positions[6] = vec2[](
vec2(-1.0, -1.0),
vec2( 1.0, -1.0),
vec2( 1.0,  1.0),

vec2( 1.0,  1.0),
vec2(-1.0,  1.0),
vec2(-1.0, -1.0)
);

vec4 color[6] = vec4[](
vec4(1.0, 0.0, 0.0, 1.0),
vec4(0.0, 1.0, 0.0, 1.0),
vec4(0.0, 0.0, 1.0, 1.0),

vec4(1.0, 0.0, 0.0, 1.0),
vec4(0.0, 1.0, 0.0, 1.0),
vec4(0.0, 0.0, 1.0, 1.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    outColor = color[gl_VertexIndex];
}