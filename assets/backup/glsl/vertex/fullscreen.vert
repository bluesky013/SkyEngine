#version 450 core
#extension GL_GOOGLE_include_directive : enable
#include <shaderlibs/glsl/vertex/fullscreen_triangle.glsl>

layout(location = 0) out vec2 outUv;

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    outUv = uv[gl_VertexIndex];
}