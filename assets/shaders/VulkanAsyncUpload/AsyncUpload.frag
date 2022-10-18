#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec2 inUv;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform sampler2D sampler0;

void main()
{
    outFragColor = texture(sampler0, inUv);
}
