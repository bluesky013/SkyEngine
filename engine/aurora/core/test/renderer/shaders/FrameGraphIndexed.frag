#version 450
layout(location = 0) in vec4 color;
layout(set = 1, binding = MATERIAL_BINDING) uniform Material { vec4 tint; } material;
layout(location = 0) out vec4 outputColor;
void main() { outputColor = color * material.tint; }
