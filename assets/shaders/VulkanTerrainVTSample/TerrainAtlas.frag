#version 450

#extension GL_ARB_sparse_texture2 : enable

layout (location = 0) in vec2 inUv;

layout (location = 0) out vec4 outFragColor;

struct QuadData {
    vec4 act;
};

layout (set = 1, binding = 0) uniform sampler2D inTexture;
layout (set = 1, binding = 1) buffer TerrainData {
    QuadData data[];
} quads;

layout (set = 2, binding = 0) uniform LocalData {
    vec4 ext;
} local;

void main()
{
    uint i = uint(local.ext.z * inUv.x);
    uint j = uint(local.ext.w * inUv.y);
    float data = quads.data[i * uint(local.ext.z) + j].act.x;

    vec4 factor = vec4(1, 1, 1, 1);
    if (data < 0.1) factor = vec4(1.0, 0.0, 0.0, 1.0);
    else if (data < 0.3) factor = vec4(0.0, 1.0, 0.0, 1.0);
    else factor = vec4(1.0, 1.0, 0.0, 1.0);

    vec4 color = vec4(0.0);
    sparseTextureARB(inTexture, inUv, color, 0);
    outFragColor = vec4(vec3(color.r), 1.0) * factor;
}
