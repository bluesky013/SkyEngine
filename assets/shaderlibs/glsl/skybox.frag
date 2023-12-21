#version 450
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec3 texCoord;

layout (location = 0) out vec4 outFragColor;

layout(set = 1, binding = 0) uniform samplerCube skybox;

#include <shaderlibs/glsl/layouts/standard_perpass.glsl>

layout (set = 0, binding = 2) uniform samplerCube irradianceMap;
layout (set = 0, binding = 3) uniform samplerCube radianceMap;
layout (set = 0, binding = 4) uniform sampler2D preFilterMap;

void main()
{
    vec4 color = texture(skybox, texCoord);
    outFragColor = vec4(pow(color.rgb, vec3(2.2)), 1.0);
}