#version 450
layout(location = 0) in vec3 texCoord;

layout (location = 0) out vec4 outFragColor;

layout(set = 1, binding = 0) uniform samplerCube skybox;

void main()
{
    outFragColor = texture(skybox, texCoord);
}