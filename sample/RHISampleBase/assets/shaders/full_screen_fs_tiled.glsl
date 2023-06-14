#version 450

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput colorImage;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 dummy1;

void main()
{
    outColor = subpassLoad(colorImage);
}