#version 450

#extension GL_ARB_sparse_texture2 : enable

layout (location = 0) in vec2 inUv;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform sampler2D inTexture;

void main()
{
    vec4 color = vec4(0.0);
    sparseTextureARB(inTexture, inUv, color, 0);
    outFragColor = color;
}
