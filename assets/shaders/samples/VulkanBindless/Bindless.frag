#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec2 inUv;
layout (location = 1) in flat int objId;

layout (location = 0) out vec4 outFragColor;

struct Material {
    vec4 baseColor;
    ivec4 texIndex;
};

layout (set = 0, binding = 0) buffer Storage {
    Material materials[];
};

layout (set = 0, binding = 1) uniform sampler   sampler0;
layout (set = 0, binding = 2) uniform texture2D textures0[];

void main()
{
    outFragColor = texture(sampler2D(textures0[materials[objId].texIndex.x], sampler0), inUv) * materials[objId].baseColor;
}
