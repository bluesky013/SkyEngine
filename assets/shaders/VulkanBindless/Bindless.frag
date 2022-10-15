#version 450

layout (location = 0) in vec2 inUv;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform sampler   sampler0;
layout (set = 0, binding = 1) uniform texture2D textures0[4];
layout (set = 0, binding = 2) uniform texture2D textures1[];

void main()
{
    if (inUv.x < 0.5 && inUv.y < 0.5) {
        outFragColor = texture(sampler2D(textures0[0], sampler0), inUv);
    } else if (inUv.x > 0.5 && inUv.y < 0.5) {
        outFragColor = texture(sampler2D(textures0[1], sampler0), inUv);
    } else if (inUv.x > 0.5 && inUv.y > 0.5) {
        outFragColor = texture(sampler2D(textures0[2], sampler0), inUv);
    } else if (inUv.x < 0.5 && inUv.y > 0.5) {
        outFragColor = texture(sampler2D(textures0[3], sampler0), inUv);
    }
}
