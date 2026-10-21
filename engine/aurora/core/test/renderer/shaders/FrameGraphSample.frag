#version 450

layout(set = 0, binding = 0) uniform texture2D sourceTex;
layout(set = 0, binding = 1) uniform sampler   sourceSampler;

layout(location = 0) out vec4 color;

void main()
{
    color = texture(sampler2D(sourceTex, sourceSampler), gl_FragCoord.xy / vec2(8.0, 8.0));
}
