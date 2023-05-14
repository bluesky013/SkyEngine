#version 450

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inColor0;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput inColor1;

layout (location = 0) out vec4 outColor0;
layout (location = 1) out vec4 outColor1;
layout (location = 2) out vec4 outColor2;

void main()
{
    vec4 v0 = subpassLoad(inColor0);
    vec4 v1 = subpassLoad(inColor1);
    vec3 color1 = v0.rgb * v0.a;
    vec3 color2 = v1.rgb * v1.a;
    outColor2 = vec4(color1 + color2, v0.a + v1.a);
}
