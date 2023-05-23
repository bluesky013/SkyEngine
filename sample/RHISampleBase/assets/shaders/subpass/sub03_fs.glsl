#version 450

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inColor0;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput inColor1;
layout (input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput inColor2;
layout (input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput inDepth;
layout (input_attachment_index = 4, set = 0, binding = 4) uniform usubpassInput inStencil;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 dummy1;
layout (location = 2) out vec4 dummy2;

void main()
{
    vec4 v0 = subpassLoad(inColor0);
    vec4 v1 = subpassLoad(inColor1);
    vec4 v2 = subpassLoad(inColor2);
    vec3 color1 = v0.rgb * v0.a;
    vec3 color2 = v1.rgb * v1.a;

    float depth = subpassLoad(inDepth).x * 1.0;
    float stencil = subpassLoad(inStencil).x / 256.0;

    outColor = v2 + vec4(color1 + color2, v0.a + v1.a);
}
