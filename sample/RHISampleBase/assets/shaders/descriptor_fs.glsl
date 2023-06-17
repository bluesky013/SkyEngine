#version 450 core

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D tex[2];
layout(set = 1, binding = 0) buffer Storage1 {
    vec4 data[];
} storage1[2];
layout(set = 1, binding = 1) uniform Matrix {
    mat4 val;
} matrix[2];
layout(set = 2, binding = 0) buffer Storage2 {
    vec4 data[];
} storage2[2];

void main()
{
    outColor = texture(tex[0], vUv) * texture(tex[1], vUv);
}