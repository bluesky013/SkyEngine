#version 450

#extension GL_ARB_sparse_texture2 : enable

layout (location = 0) in vec2 inUv;

layout (location = 0) out vec4 outFragColor;

struct QuadData {
    uint level;
    uint offsetX;
    uint offsetY;
    uint padding3;
};

layout (set = 1, binding = 0) uniform sampler2D inTexture;
layout (set = 1, binding = 1) buffer TerrainData {
    QuadData data[];
} quads;

layout (set = 2, binding = 0) uniform LocalData {
    vec4 ext;
} local;

#define EP 2

void main()
{
    uint i = uint(local.ext.z * inUv.x);
    uint j = uint(local.ext.w * inUv.y);

    uint x = uint(local.ext.x * inUv.x);
    uint y = uint(local.ext.y * inUv.y);

    uint m = uint(local.ext.x / local.ext.z);
    uint n = uint(local.ext.y / local.ext.w);

    QuadData quad = quads.data[j * uint(local.ext.z) + i];
    uint level = quad.level;
    if (((x % m) < EP) || ((y % n) < EP) || (((x + EP) % m) < EP) || (((y + EP) % n) < EP)) {
        outFragColor = vec4(1, 1, 0, 1);
        return;
    }
    vec4 factor = vec4(1, 1, 1, 1);
    if (level == 0) factor = vec4(1.0, 0.0, 0.0, 1.0);
    else if (level == 1) factor = vec4(0.0, 1.0, 0.0, 1.0);

    vec4 color = vec4(0.0);
    sparseTextureARB(inTexture, inUv, color, 0);
    outFragColor = vec4(vec3(color.r), 1.0) * factor;
}
