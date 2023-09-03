#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 tangent;
layout (location = 3) in vec3 biTangent;
layout (location = 4) in vec4 color;
layout (location = 5) in vec2 uv;

layout (location = 0) out vec4 outFragColor;

layout (set = 1, binding = 0) uniform Material {
    vec4 baseColor;
};
layout (set = 1, binding = 1) uniform sampler2D mainColor;


void main()
{
    vec3 L = normalize(vec3(0, 20, 0) - pos);
    vec3 N = normalize(normal);
    float nDotL = clamp(dot(N, L), 0, 1);
    outFragColor = color * baseColor * texture(mainColor, uv) * nDotL;
}
