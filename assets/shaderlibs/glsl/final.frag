#version 450
layout(location = 0) in vec2 uv;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform sampler2D colorImage;


// Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
vec3 aces(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec4 color = texture(colorImage, uv);
    // tone mapping
    vec3 ldr = aces(color.xyz);

    outFragColor = vec4(pow(ldr, vec3(1.0f / 2.2)), color.a);
}